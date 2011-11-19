#define _WIN32_WINNT 0x0501
#include "Agent.h"
#include "Win32Console.h"
#include "../Shared/DebugClient.h"
#include "../Shared/AgentMsg.h"
#include <QCoreApplication>
#include <QLocalSocket>
#include <QtDebug>
#include <QTimer>
#include <QSize>
#include <QRect>
#include <string.h>
#include <windows.h>

const int SC_CONSOLE_MARK = 0xFFF2;
const int SYNC_MARKER_LEN = 16;
#define CSI "\x1b["

Agent::Agent(const QString &socketServer,
             int initialCols,
             int initialRows,
             QObject *parent) :
    QObject(parent),
    m_timer(NULL),
    m_syncRow(-1),
    m_syncCounter(0),
    m_scrapedLineCount(0),
    m_scrolledCount(0),
    m_maxBufferedLine(0),
    m_remoteLine(0)
{
    m_bufferData = new CHAR_INFO[BUFFER_LINE_COUNT][MAX_CONSOLE_WIDTH];
    memset(m_bufferData, 0, sizeof(CHAR_INFO) * BUFFER_LINE_COUNT * MAX_CONSOLE_WIDTH);

    m_console = new Win32Console(this);
    m_console->reposition(
                QSize(initialCols, BUFFER_LINE_COUNT),
                QRect(0, 0, initialCols, initialRows));
    m_console->setCursorPosition(QPoint(0, 0));

    // Connect to the named pipe.
    m_socket = new QLocalSocket(this);
    m_socket->connectToServer(socketServer);
    if (!m_socket->waitForConnected())
        qFatal("Could not connect to %s", socketServer.toStdString().c_str());
    m_socket->setReadBufferSize(64*1024);

    connect(m_socket, SIGNAL(readyRead()), SLOT(socketReadyRead()));
    connect(m_socket, SIGNAL(disconnected()), SLOT(socketDisconnected()));

    m_timer = new QTimer(this);
    m_timer->setSingleShot(false);
    connect(m_timer, SIGNAL(timeout()), SLOT(pollTimeout()));
    m_timer->start(25);

    Trace("agent starting...");
}

Agent::~Agent()
{
    m_console->postCloseMessage();
    delete [] m_bufferData;
}

void Agent::socketReadyRead()
{
    // TODO: This is an incomplete hack...
    Trace("socketReadyRead -- %d bytes available", m_socket->bytesAvailable());
    while (m_socket->bytesAvailable() >= sizeof(AgentMsg)) {
        AgentMsg msg;
        m_socket->read((char*)&msg, sizeof(msg));
        switch (msg.type) {
            case AgentMsg::InputRecord: {
                m_console->writeInput(&msg.u.inputRecord);
                break;
            }
            case AgentMsg::WindowSize: {
                AgentMsg nextMsg;
                if (m_socket->peek((char*)&nextMsg, sizeof(nextMsg)) == sizeof(nextMsg) &&
                        nextMsg.type == AgentMsg::WindowSize) {
                    // Two consecutive window resize requests.  Windows seems
                    // to be really slow at resizing a console, so ignore the
                    // first resize operation.  This idea didn't work as well
                    // as I'd hoped it would, but I suppose I'll leave it in.
                    Trace("skipping");
                    continue;
                }
                Trace("resize started");
                resizeWindow(msg.u.windowSize.cols, msg.u.windowSize.rows);
                Trace("resize done");
                break;
            }
        }
    }
    Trace("socketReadyRead -- exited");
}

void Agent::socketDisconnected()
{
    QCoreApplication::exit(0);
}

void Agent::pollTimeout()
{
    if (m_socket->state() == QLocalSocket::ConnectedState) {
        DWORD dummy;
        int count = GetConsoleProcessList(&dummy, 1);
        Q_ASSERT(count >= 1);
        scrapeOutput();
        if (count == 1) {
            // TODO: This approach doesn't seem to work when I run edit.com
            // in a console.  I see an NTVDM.EXE process running after exiting
            // cmd.exe.  Maybe NTVDM.EXE is doing the same thing I am -- it
            // sees that there's still another process in the console process
            // list, so it stays running.

            Trace("No real processes in Console -- start shut down");
            m_socket->disconnectFromServer();
        }
    } else {
        m_timer->stop();
    }
}

void Agent::resizeWindow(int cols, int rows)
{
    freezeConsole();

    QSize bufferSize = m_console->bufferSize();
    QRect windowRect = m_console->windowRect();
    QSize newBufferSize(cols, bufferSize.height());
    QRect newWindowRect;

    // This resize behavior appears to match what happens when I resize the
    // console window by hand.
    if (windowRect.top() + windowRect.height() == bufferSize.height() ||
            windowRect.top() + rows >= bufferSize.height()) {
        // Lock the bottom of the new window to the bottom of the buffer if either
        //  - the window was already at the bottom of the buffer, OR
        //  - there isn't enough room.
        newWindowRect = QRect(0, newBufferSize.height() - rows, cols, rows);
    } else {
        // Keep the top of the window where it is.
        newWindowRect = QRect(0, windowRect.top(), cols, rows);
    }

    QSize tempBufferSize = bufferSize;
    tempBufferSize.setWidth(std::max(bufferSize.width(), newBufferSize.width()));

    m_console->reposition(tempBufferSize, newWindowRect);
    unfreezeConsole();
    if (newBufferSize.width() != tempBufferSize.width())
        m_console->resizeBuffer(newBufferSize);
}

void Agent::scrapeOutput()
{
    // Get the location of the console prior to freezing.  The current
    // freezing technique, marking, temporarily moves the cursor to the
    // top-left.  We need to know the real cursor position.
    QPoint cursor = m_console->cursorPosition();

    freezeConsole();
    hideTerminalCursor();

    if (m_syncRow != -1) {
        // If a synchronizing marker was placed into the history, look for it
        // and adjust the scroll count.
        int markerRow = findSyncMarker();
        if (markerRow == -1) {
            // TODO: Something happened.  Reset the terminal....
        } else {
            m_scrolledCount += (m_syncRow - markerRow);
            m_syncRow = markerRow;
        }
    }

    QRect windowRect = m_console->windowRect();

    int firstLine = std::min(m_scrapedLineCount,
                             windowRect.top() + m_scrolledCount);
    int lastLine = windowRect.top() + windowRect.height() - 1 + m_scrolledCount;

    for (int line = firstLine; line <= lastLine; ++line) {
        CHAR_INFO lineData[500]; // TODO: bufoverflow
        QRect lineRect(0, line - m_scrolledCount, windowRect.width(), 1);
        m_console->read(lineRect, lineData);

        // TODO: The memcpy can overflow the m_bufferData buffer.
        if (line > m_maxBufferedLine ||
                memcmp(lineData,
                       m_bufferData[line % BUFFER_LINE_COUNT],
                       sizeof(CHAR_INFO) * windowRect.width()) != 0) {
            sendLineToTerminal(line, lineData, windowRect.width());
            memset(m_bufferData[line % BUFFER_LINE_COUNT],
                   0,
                   sizeof(m_bufferData[line % BUFFER_LINE_COUNT]));
            memcpy(m_bufferData[line % BUFFER_LINE_COUNT],
                   lineData,
                   sizeof(CHAR_INFO) * windowRect.width());
            m_maxBufferedLine = std::max(m_maxBufferedLine, line);
        }
    }

    m_scrapedLineCount = windowRect.top() + m_scrolledCount;

    if (windowRect.top() > 200) { // TODO: replace hard-coded constant
        createSyncMarker(windowRect.top() - 200);
    }

    showTerminalCursor(cursor.y() + m_scrolledCount, cursor.x());

    unfreezeConsole();
}

void Agent::freezeConsole()
{
    SendMessage(m_console->hwnd(), WM_SYSCOMMAND, SC_CONSOLE_MARK, 0);
}

void Agent::unfreezeConsole()
{
    SendMessage(m_console->hwnd(), WM_CHAR, 27, 0x00010001);
}

void Agent::syncMarkerText(CHAR_INFO *output)
{
    char str[SYNC_MARKER_LEN + 1];// TODO: use a random string
    sprintf(str, "S*Y*N*C*%08x", m_syncCounter);
    memset(output, 0, sizeof(CHAR_INFO) * SYNC_MARKER_LEN);
    for (int i = 0; i < SYNC_MARKER_LEN; ++i) {
        output[i].Char.AsciiChar = str[i];
        output[i].Attributes = 7;
    }
}

int Agent::findSyncMarker()
{
    Q_ASSERT(m_syncRow >= 0);
    CHAR_INFO marker[SYNC_MARKER_LEN];
    CHAR_INFO column[BUFFER_LINE_COUNT];
    syncMarkerText(marker);
    QRect rect(0, 0, 1, m_syncRow + SYNC_MARKER_LEN);
    m_console->read(rect, column);
    int i;
    for (i = m_syncRow; i >= 0; --i) {
        int j;
        for (j = 0; j < SYNC_MARKER_LEN; ++j) {
            if (column[i + j].Char.AsciiChar != marker[j].Char.AsciiChar)
                break;
        }
        if (j == SYNC_MARKER_LEN)
            return i;
    }
    return -1;
}

void Agent::createSyncMarker(int row)
{
    // Write a new marker.
    m_syncCounter++;
    CHAR_INFO marker[SYNC_MARKER_LEN];
    syncMarkerText(marker);
    m_syncRow = row;
    QRect markerRect(0, m_syncRow, 1, SYNC_MARKER_LEN);
    m_console->write(markerRect, marker);
}

void Agent::moveTerminalToLine(int line)
{
    // Do not use CPL or CNL.  Konsole 2.5.4 does not support Cursor Previous
    // Line (CPL) -- there are "Undecodable sequence" errors.  gnome-terminal
    // 2.32.0 does handle it.  Cursor Next Line (CNL) does nothing if the
    // cursor is on the last line already.

    if (line < m_remoteLine) {
        // Cursor Horizontal Absolute (CHA) -- move to column 1.
        // CUrsor Up (CUU)
        char buffer[32];
        sprintf(buffer, CSI"1G"CSI"%dA", m_remoteLine - line);
        m_socket->write(buffer);
        m_remoteLine = line;
    } else if (line > m_remoteLine) {
        while (line > m_remoteLine) {
            m_socket->write("\r\n");
            m_remoteLine++;
        }
    } else {
        // Cursor Horizontal Absolute -- move to column 1.
        m_socket->write(CSI"1G");
    }
}

void Agent::sendLineToTerminal(int line, CHAR_INFO *lineData, int width)
{
    moveTerminalToLine(line);

    // Erase in Line -- erase entire line.
    m_socket->write(CSI"2K");

    char buffer[512];
    int length = 0;
    for (int i = 0; i < width; ++i) {
        buffer[i] = lineData[i].Char.AsciiChar;
        if (buffer[i] != ' ')
            length = i + 1;
    }

    m_socket->write(buffer, length);
}

void Agent::hideTerminalCursor()
{
    m_socket->write(CSI"?25l");
}

void Agent::showTerminalCursor(int line, int column)
{
    moveTerminalToLine(line);
    char buffer[32];
    sprintf(buffer, CSI"%dG"CSI"?25h", column + 1);
    m_socket->write(buffer);
}
