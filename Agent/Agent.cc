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
const int DEFAULT_WINDOW_COLS = 80;
const int DEFAULT_WINDOW_ROWS = 25;

Agent::Agent(const QString &socketServer, QObject *parent) : QObject(parent)
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

    // TODO: The agent should probably have an initial window size from the
    // client/window, but it currently doesn't, so default to 80x25.
    COORD bufferSize = { DEFAULT_WINDOW_COLS, BUFFER_LINE_COUNT };
    SetConsoleScreenBufferSize(m_conout, bufferSize);
    SMALL_RECT windowPos = {
        0,
        BUFFER_LINE_COUNT - DEFAULT_WINDOW_ROWS,
        DEFAULT_WINDOW_COLS - 1,
        BUFFER_LINE_COUNT - 1,
    };
    SetConsoleWindowInfo(m_conout, TRUE, &windowPos);
    COORD initialCursorPos = { windowPos.Left, windowPos.Top };
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
                resizeWindow(msg.u.windowSize.cols, msg.u.windowSize.rows);
                break;
            }
        }
    }
}

void Agent::socketDisconnected()
{
    QCoreApplication::exit(0);
}

void Agent::resizeWindow(unsigned short cols, unsigned short rows)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(m_conout, &info);

    COORD bufferSize = { cols, BUFFER_LINE_COUNT };
    SMALL_RECT windowPos = {
        0,
        BUFFER_LINE_COUNT - rows,
        cols - 1,
        BUFFER_LINE_COUNT - 1,
    };
    if (cols > info.dwSize.X) {
        SetConsoleScreenBufferSize(m_conout, bufferSize);
        SetConsoleWindowInfo(m_conout, TRUE, &windowPos);
    } else {
        SetConsoleWindowInfo(m_conout, TRUE, &windowPos);
        SetConsoleScreenBufferSize(m_conout, bufferSize);
    }
    // Don't move the cursor, even if the cursor is now off the screen.
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
    Trace("Agent poll -- writing %d bytes", pwrite - writeBuffer);
    m_socket->write(writeBuffer, pwrite - writeBuffer);
}
