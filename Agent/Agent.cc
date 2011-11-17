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

const int BUFFER_LINE_COUNT = 5000;

Agent::Agent(const QString &socketServer,
             int initialCols,
             int initialRows,
             QObject *parent) :
    QObject(parent), m_timer(NULL)
{
    m_console = new Win32Console(this);
    resizeWindow(initialCols, initialRows);
    m_console->setCursorPosition(QPoint(0, BUFFER_LINE_COUNT - initialRows));

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
    m_console->postCloseMessage();
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
    m_console->reposition(
                QSize(cols, BUFFER_LINE_COUNT),
                QRect(0, BUFFER_LINE_COUNT - rows, cols, rows));
}

void Agent::scrapeOutput()
{
    const QRect windowRect = m_console->windowRect();
    CHAR_INFO readBuffer[64 * 1024 / sizeof(CHAR_INFO)]; // TODO: buf overflow
    char writeBuffer[sizeof(readBuffer) / sizeof(CHAR_INFO)]; // TODO: buf overflow
    char *pwrite = writeBuffer;
    m_console->read(windowRect, readBuffer);

    // Simplest algorithm -- just send the whole screen.
    for (int y = 0; y < windowRect.height(); ++y) {
        for (int x = 0; x < windowRect.width(); ++x) {
            CHAR_INFO *pch = &readBuffer[y * windowRect.width() + x];
            *pwrite++ = pch->Char.AsciiChar;
        }
        if (y < windowRect.height() - 1) {
            *pwrite++ = '\r';
            *pwrite++ = '\n';
        }
    }
    //Trace("Agent poll -- writing %d bytes", pwrite - writeBuffer);
    m_socket->write(writeBuffer, pwrite - writeBuffer);
}
