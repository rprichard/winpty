#include "Agent.h"
#include "Win32Console.h"
#include "Terminal.h"
#include "NamedPipe.h"
#include "../Shared/DebugClient.h"
#include "../Shared/AgentMsg.h"
#include "../Shared/Buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <vector>
#include <string>

const int SC_CONSOLE_MARK = 0xFFF2;
const int SC_CONSOLE_SELECT_ALL = 0xFFF5;
const int SYNC_MARKER_LEN = 16;

Agent::Agent(LPCWSTR controlPipeName,
             LPCWSTR dataPipeName,
             int initialCols,
             int initialRows) :
    m_terminal(NULL),
    m_timer(NULL),
    m_childProcess(NULL),
    m_childExitCode(-1),
    m_syncCounter(0)
{
    m_bufferData = new CHAR_INFO[BUFFER_LINE_COUNT][MAX_CONSOLE_WIDTH];

    m_console = new Win32Console;
    m_console->reposition(
                Coord(initialCols, BUFFER_LINE_COUNT),
                SmallRect(0, 0, initialCols, initialRows));
    m_console->setCursorPosition(Coord(0, 0));

    m_controlSocket = makeSocket(controlPipeName);
    m_dataSocket = makeSocket(dataPipeName);
    m_terminal = new Terminal(m_dataSocket);

    resetConsoleTracking(false);

    //connect(m_controlSocket, SIGNAL(readyRead()), SLOT(controlSocketReadyRead()));
    //connect(m_controlSocket, SIGNAL(disconnected()), SLOT(socketDisconnected()));
    //connect(m_dataSocket, SIGNAL(readyRead()), SLOT(dataSocketReadyRead()));

    //m_timer = new QTimer(this);
    //m_timer->setSingleShot(false);
    //connect(m_timer, SIGNAL(timeout()), SLOT(pollTimeout()));
    //m_timer->start(25);
    setPollInterval(25);

    Trace("agent starting...");
}

Agent::~Agent()
{
    // TODO: review how shut down and cleanup work.

    m_console->postCloseMessage();

    delete [] m_bufferData;
    delete m_console;
    delete m_terminal;
}

NamedPipe *Agent::makeSocket(LPCWSTR pipeName)
{
    NamedPipe *pipe = createNamedPipe();
    if (!pipe->connectToServer(pipeName)) {
        Trace("error: could not connect to %ls", pipeName);
        ::exit(1);
    }
    pipe->setReadBufferSize(64 * 1024);
    return pipe;
}

void Agent::resetConsoleTracking(bool sendClear)
{
    memset(m_bufferData, 0, sizeof(CHAR_INFO) * BUFFER_LINE_COUNT * MAX_CONSOLE_WIDTH);
    m_syncRow = -1;
    m_scrapedLineCount = m_console->windowRect().top();
    m_scrolledCount = 0;
    m_maxBufferedLine = -1;
    m_dirtyWindowTop = -1;
    m_dirtyLineCount = 0;
    m_terminal->reset(sendClear, m_scrapedLineCount);
}

void Agent::onPipeIo()
{
    controlSocketReadyRead();
    dataSocketReadyRead();

    // TODO: Is it possible that one or more pipe has closed when this
    // function returns?  Will the agent shut down correctly?  I don't see any
    // calls to EventLoop::exit.
}

void Agent::controlSocketReadyRead()
{
    while (true) {
        int32_t packetSize;
        int size = m_controlSocket->peek((char*)&packetSize, sizeof(int32_t));
        if (size < (int)sizeof(int32_t))
            break;
        int totalSize = sizeof(int32_t) + packetSize;
        if (m_controlSocket->bytesAvailable() < totalSize) {
            if (m_controlSocket->readBufferSize() < totalSize)
                m_controlSocket->setReadBufferSize(totalSize);
            break;
        }
        std::string packetData = m_controlSocket->read(totalSize);
        assert((int)packetData.size() == totalSize);
        ReadBuffer buffer(packetData);
        buffer.getInt(); // Discard the size.
        Trace("read packet of %d total bytes", totalSize);
        handlePacket(buffer);
    }
}

void Agent::handlePacket(ReadBuffer &packet)
{
    int type = packet.getInt();
    int32_t result = -1;
    switch (type) {
    case AgentMsg::StartProcess:
        result = handleStartProcessPacket(packet);
        break;
    case AgentMsg::SetSize:
        result = handleSetSizePacket(packet);
        break;
    case AgentMsg::GetExitCode:
        packet.assertEof();
        result = m_childExitCode;
    default:
        Trace("Unrecognized message, id:%d", type);
    }
    m_controlSocket->write((char*)&result, sizeof(result));
}

int Agent::handleStartProcessPacket(ReadBuffer &packet)
{
    assert(m_childProcess == NULL);

    std::wstring program = packet.getWString();
    std::wstring cmdline = packet.getWString();
    std::wstring cwd = packet.getWString();
    std::wstring env = packet.getWString();
    packet.assertEof();

    LPCWSTR programArg = program.empty() ? NULL : program.c_str();
    std::vector<wchar_t> cmdlineCopy;
    LPWSTR cmdlineArg = NULL;
    if (!cmdline.empty()) {
        cmdlineCopy.resize(cmdline.size() + 1);
        cmdline.copy(&cmdlineCopy[0], cmdline.size());
        cmdlineCopy[cmdline.size()] = L'\0';
        cmdlineArg = &cmdlineCopy[0];
    }
    LPCWSTR cwdArg = cwd.empty() ? NULL : cwd.c_str();
    LPCWSTR envArg = env.empty() ? NULL : env.data();
    STARTUPINFO sui;
    PROCESS_INFORMATION pi;
    memset(&sui, 0, sizeof(sui));
    memset(&pi, 0, sizeof(pi));
    sui.cb = sizeof(STARTUPINFO);

    BOOL ret = CreateProcess(programArg, cmdlineArg, NULL, NULL,
                             /*bInheritHandles=*/FALSE,
                             /*dwCreationFlags=*/CREATE_UNICODE_ENVIRONMENT,
                             (LPVOID)envArg, cwdArg, &sui, &pi);

    Trace("cp: %s %d", (ret ? "success" : "fail"), (int)pi.dwProcessId);

    if (ret) {
        CloseHandle(pi.hThread);
        m_childProcess = pi.hProcess;
    }

    // TODO: report success/failure to client
    return ret ? 0 : GetLastError();
}

int Agent::handleSetSizePacket(ReadBuffer &packet)
{
    Trace("SetSize msg");
    int cols = packet.getInt();
    int rows = packet.getInt();
    packet.assertEof();
    resizeWindow(cols, rows);
    return 0;
}

void Agent::dataSocketReadyRead()
{
    // TODO: This is an incomplete hack...
    Trace("socketReadyRead -- %d bytes available", m_dataSocket->bytesAvailable());
    std::string data = m_dataSocket->readAll();
    for (size_t i = 0; i < data.size(); ++i) {
        char ch = data[i];
        const short vk = VkKeyScan(ch);
        if (vk != -1) {
            INPUT_RECORD ir;
            memset(&ir, 0, sizeof(ir));
            ir.EventType = KEY_EVENT;
            ir.Event.KeyEvent.bKeyDown = TRUE;
            ir.Event.KeyEvent.wVirtualKeyCode = vk & 0xff;
            ir.Event.KeyEvent.wVirtualScanCode = 0;
            ir.Event.KeyEvent.uChar.AsciiChar = ch;
            ir.Event.KeyEvent.wRepeatCount = 1;
            m_console->writeInput(&ir);
        }
    }
}

void Agent::socketDisconnected()
{
    //QCoreApplication::exit(0);
}

void Agent::onPollTimeout()
{
    if (/*TODO: stop scraping as we're shutting down*/true)
        scrapeOutput();

    if (m_childProcess != NULL) {
        if (WaitForSingleObject(m_childProcess, 0) == WAIT_OBJECT_0) {
            DWORD exitCode;
            if (GetExitCodeProcess(m_childProcess, &exitCode))
                m_childExitCode = exitCode;
            CloseHandle(m_childProcess);
            m_childProcess = NULL;
            // TODO: review how exiting/disconnecting work...
            //m_dataSocket->disconnectFromServer();
            m_dataSocket->closePipe();
        }
    }
}

// Detect window movement.  If the window moves down (presumably as a
// result of scrolling), then assume that all screen buffer lines down to
// the bottom of the window are dirty.
void Agent::markEntireWindowDirty()
{
    SmallRect windowRect = m_console->windowRect();
    m_dirtyLineCount = std::max(m_dirtyLineCount,
                                windowRect.top() + windowRect.height());
}

// Scan the screen buffer and advance the dirty line count when we find
// non-empty lines.
void Agent::scanForDirtyLines()
{
    const SmallRect windowRect = m_console->windowRect();
    CHAR_INFO prevChar;
    if (m_dirtyLineCount >= 1) {
        m_console->read(SmallRect(windowRect.width() - 1,
                                  m_dirtyLineCount - 1,
                                  1, 1),
                        &prevChar);
    } else {
        m_console->read(SmallRect(0, 0, 1, 1), &prevChar);
    }
    int attr = prevChar.Attributes;

    for (int line = m_dirtyLineCount;
         line < windowRect.top() + windowRect.height();
         ++line) {
        CHAR_INFO lineData[MAX_CONSOLE_WIDTH]; // TODO: bufoverflow
        SmallRect lineRect(0, line, windowRect.width(), 1);
        m_console->read(lineRect, lineData);
        for (int col = 0; col < windowRect.width(); ++col) {
            int newAttr = lineData[col].Attributes;
            if (lineData[col].Char.AsciiChar != ' ' || attr!= newAttr)
                m_dirtyLineCount = line + 1;
            newAttr = attr;
        }
    }
}

void Agent::resizeWindow(int cols, int rows)
{
    freezeConsole();

    Coord bufferSize = m_console->bufferSize();
    SmallRect windowRect = m_console->windowRect();
    Coord newBufferSize(cols, bufferSize.Y);
    SmallRect newWindowRect;

    // This resize behavior appears to match what happens when I resize the
    // console window by hand.
    if (windowRect.top() + windowRect.height() == bufferSize.Y ||
            windowRect.top() + rows >= bufferSize.Y) {
        // Lock the bottom of the new window to the bottom of the buffer if either
        //  - the window was already at the bottom of the buffer, OR
        //  - there isn't enough room.
        newWindowRect = SmallRect(0, newBufferSize.Y - rows, cols, rows);
    } else {
        // Keep the top of the window where it is.
        newWindowRect = SmallRect(0, windowRect.top(), cols, rows);
    }

    if (m_dirtyWindowTop != -1 && m_dirtyWindowTop < windowRect.top())
        markEntireWindowDirty();
    m_dirtyWindowTop = newWindowRect.top();

    m_console->reposition(newBufferSize, newWindowRect);
    unfreezeConsole();
}

void Agent::scrapeOutput()
{
    freezeConsole();

    const Coord cursor = m_console->cursorPosition();
    const SmallRect windowRect = m_console->windowRect();

    if (m_syncRow != -1) {
        // If a synchronizing marker was placed into the history, look for it
        // and adjust the scroll count.
        int markerRow = findSyncMarker();
        if (markerRow == -1) {
            // Something has happened.  Reset the terminal.
            Trace("Sync marker has disappeared -- resetting the terminal");
            resetConsoleTracking();
        } else if (markerRow != m_syncRow) {
            assert(markerRow < m_syncRow);
            m_scrolledCount += (m_syncRow - markerRow);
            m_syncRow = markerRow;
            // If the buffer has scrolled, then the entire window is dirty.
            markEntireWindowDirty();
        }
    }

    // Update the dirty line count:
    //  - If the window has moved, the entire window is dirty.
    //  - Everything up to the cursor is dirty.
    //  - All lines above the window are dirty.
    //  - Any non-blank lines are dirty.
    if (m_dirtyWindowTop != -1) {
        if (windowRect.top() > m_dirtyWindowTop) {
            // The window has moved down, presumably as a result of scrolling.
            markEntireWindowDirty();
        } else if (windowRect.top() < m_dirtyWindowTop) {
            // The window has moved upward.  This is generally not expected to
            // happen, but the CMD/PowerShell CMD command will move the window
            // to the top as part of clearing everything else in the console.
            Trace("Window moved upward -- resetting the terminal");
            resetConsoleTracking();
        }
    }
    m_dirtyWindowTop = windowRect.top();
    m_dirtyLineCount = std::max(m_dirtyLineCount, cursor.Y + 1);
    m_dirtyLineCount = std::max(m_dirtyLineCount, (int)windowRect.top());
    scanForDirtyLines();

    // Note that it's possible for all the lines on the current window to
    // be non-dirty.

    int firstLine = std::min(m_scrapedLineCount,
                             windowRect.top() + m_scrolledCount);
    int stopLine = std::min(m_dirtyLineCount,
                            windowRect.top() + windowRect.height()) +
            m_scrolledCount;

    bool sawModifiedLine = false;

    for (int line = firstLine; line < stopLine; ++line) {
        CHAR_INFO curLine[MAX_CONSOLE_WIDTH]; // TODO: bufoverflow
        const int w = windowRect.width();
        m_console->read(SmallRect(0, line - m_scrolledCount, w, 1), curLine);

        // TODO: The memcpy can overflow the m_bufferData buffer.
        CHAR_INFO (&bufLine)[MAX_CONSOLE_WIDTH] =
                m_bufferData[line % BUFFER_LINE_COUNT];
        if (sawModifiedLine ||
                line > m_maxBufferedLine ||
                memcmp(curLine, bufLine, sizeof(CHAR_INFO) * w) != 0) {
            //Trace("sent line %d", line);
            m_terminal->sendLine(line, curLine, windowRect.width());
            memset(bufLine, 0, sizeof(bufLine));
            memcpy(bufLine, curLine, sizeof(CHAR_INFO) * w);
            for (int col = w; col < MAX_CONSOLE_WIDTH; ++col) {
                bufLine[col].Attributes = curLine[w - 1].Attributes;
                bufLine[col].Char.AsciiChar = ' ';
            }
            m_maxBufferedLine = std::max(m_maxBufferedLine, line);
            sawModifiedLine = true;
        }
    }

    m_scrapedLineCount = windowRect.top() + m_scrolledCount;

    if (windowRect.top() > 200) { // TODO: replace hard-coded constant
        createSyncMarker(windowRect.top() - 200);
    }

    m_terminal->finishOutput(cursor + Coord(0, m_scrolledCount));

    unfreezeConsole();
}

void Agent::freezeConsole()
{
    SendMessage(m_console->hwnd(), WM_SYSCOMMAND, SC_CONSOLE_SELECT_ALL, 0);
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
    assert(m_syncRow >= 0);
    CHAR_INFO marker[SYNC_MARKER_LEN];
    CHAR_INFO column[BUFFER_LINE_COUNT];
    syncMarkerText(marker);
    SmallRect rect(0, 0, 1, m_syncRow + SYNC_MARKER_LEN);
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
    SmallRect markerRect(0, m_syncRow, 1, SYNC_MARKER_LEN);
    m_console->write(markerRect, marker);
}
