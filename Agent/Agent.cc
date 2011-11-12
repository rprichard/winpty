#define _WIN32_WINNT 0x0501
#include "Agent.h"
#include "../Shared/DebugClient.h"
#include <QCoreApplication>
#include <QLocalSocket>
#include <QtDebug>
#include <QTimer>
#include <string.h>
#include <windows.h>

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
    while (m_socket->bytesAvailable() >= sizeof(INPUT_RECORD)) {
        INPUT_RECORD ir;
        m_socket->read((char*)&ir, sizeof(ir));
        DWORD dummy;
        WriteConsoleInput(m_conin, &ir, 1, &dummy);
    }
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
        *pwrite++ = '\r';
        *pwrite++ = '\n';
    }
    Trace("Agent poll -- writing %d bytes", pwrite - writeBuffer);
    m_socket->write(writeBuffer, pwrite - writeBuffer);
}
