#define _WIN32_WINNT 0x0501
#include "Agent.h"
#include "../Shared/DebugClient.h"
#include "../Shared/AgentMsg.h"
#include <QCoreApplication>
#include <QLocalSocket>
#include <QtDebug>
#include <QTimer>
#include <string.h>
#include <windows.h>

const int BUFFER_LINE_COUNT = 5000;

static void resizeWindow(HANDLE conout, unsigned short cols, unsigned short rows)
{
    // Windows has one API for resizing the screen buffer and a different one
    // for resizing the window.  It seems that either API can fail if the
    // window does not fit on the screen buffer.

    CONSOLE_SCREEN_BUFFER_INFO originalInfo;
    GetConsoleScreenBufferInfo(conout, &originalInfo);

    COORD finalBufferSize = { cols, BUFFER_LINE_COUNT };
    SMALL_RECT finalWindowRect = {
        0,
        BUFFER_LINE_COUNT - rows,
        cols - 1,
        BUFFER_LINE_COUNT - 1,
    };

    if (originalInfo.dwSize.Y != BUFFER_LINE_COUNT) {
        // TODO: Is it really safe to resize the window down to 1x1?
        // TODO: Is there a better way to do this?
        SMALL_RECT smallestWindowRect = { 0, 0, 0, 0 };
        SetConsoleWindowInfo(conout, TRUE, &smallestWindowRect);
        SetConsoleScreenBufferSize(conout, finalBufferSize);
        SetConsoleWindowInfo(conout, TRUE, &finalWindowRect);
    } else {
        if (cols > originalInfo.dwSize.X) {
            SetConsoleScreenBufferSize(conout, finalBufferSize);
            SetConsoleWindowInfo(conout, TRUE, &finalWindowRect);
        } else {
            SetConsoleWindowInfo(conout, TRUE, &finalWindowRect);
            SetConsoleScreenBufferSize(conout, finalBufferSize);
        }
    }
    // Don't move the cursor, even if the cursor is now off the screen.
}

Agent::Agent(const QString &socketServer,
             int initialCols,
             int initialRows,
             QObject *parent) :
    QObject(parent), m_timer(NULL)
{
    m_conin = CreateFile(
                L"CONIN$",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_EXISTING, 0, NULL);
    m_conout = CreateFile(
                L"CONOUT$",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_EXISTING, 0, NULL);
    Q_ASSERT(m_conin != NULL);
    Q_ASSERT(m_conout != NULL);

    resizeWindow(m_conout, initialCols, initialRows);
    COORD initialCursorPos = { 0, BUFFER_LINE_COUNT - initialRows };
    SetConsoleCursorPosition(m_conout, initialCursorPos);

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
    m_timer->start(100);

    Trace("agent starting...");
}

Agent::~Agent()
{
    HWND hwnd = GetConsoleWindow();
    if (hwnd != NULL)
        PostMessage(hwnd, WM_CLOSE, 0, 0);
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
                DWORD dummy;
                WriteConsoleInput(m_conin, &msg.u.inputRecord, 1, &dummy);
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
                resizeWindow(m_conout, msg.u.windowSize.cols, msg.u.windowSize.rows);
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

void Agent::scrapeOutput()
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(m_conout, &info)) {
        Trace("GetConsoleScreenBufferInfo failed");
        return;
    }

    COORD size;
    size.X = info.srWindow.Right - info.srWindow.Left + 1;
    size.Y = info.srWindow.Bottom - info.srWindow.Top + 1;
    COORD zeroPos = {0, 0};
    SMALL_RECT readWindow = info.srWindow;

    CHAR_INFO readBuffer[64 * 1024 / sizeof(CHAR_INFO)]; // TODO: buf overflow
    if (!ReadConsoleOutput(m_conout, readBuffer, size, zeroPos, &readWindow)) {
        Trace("ReadConsoleOutput failed");
        return;
    }

    char writeBuffer[sizeof(readBuffer) / sizeof(CHAR_INFO)]; // TODO: buf overflow
    char *pwrite = writeBuffer;
    if (memcmp(&info.srWindow, &readWindow, sizeof(SMALL_RECT))) {
        Trace("ReadConsoleOutput returned a different-sized buffer");
        return;
    }

    // Simplest algorithm -- just send the whole screen.
    for (int y = 0; y < size.Y; ++y) {
        for (int x = 0; x < size.X; ++x) {
            CHAR_INFO *pch = &readBuffer[y * size.X + x];
            *pwrite++ = pch->Char.AsciiChar;
        }
        if (y < size.Y - 1) {
            *pwrite++ = '\r';
            *pwrite++ = '\n';
        }
    }
    //Trace("Agent poll -- writing %d bytes", pwrite - writeBuffer);
    m_socket->write(writeBuffer, pwrite - writeBuffer);
}
