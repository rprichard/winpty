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
        qFatal("Could not connect to %s", socketServer.toAscii().data());
    m_socket->setReadBufferSize(64*1024);

    connect(m_socket, SIGNAL(readyRead()), SLOT(socketReadyRead()));
    connect(m_socket, SIGNAL(readChannelFinished()), SLOT(socketClosed()));

    m_timer = new QTimer(this);
    m_timer->setSingleShot(false);
    connect(m_timer, SIGNAL(timeout()), SLOT(pollTimeout()));
    m_timer->start(100);

    Trace("agent starting...");
}

void Agent::socketReadyRead()
{
    // TODO: This is an incomplete hack...
    while (m_socket->bytesAvailable() >= sizeof(INPUT_RECORD)) {
        Trace("reading a word...");
        INPUT_RECORD ir;
        m_socket->read((char*)&ir, sizeof(ir));
        DWORD dummy;
        WriteConsoleInput(m_conin, &ir, 1, &dummy);
    }
}

void Agent::socketClosed()
{
    // TODO: cleanup?
    QCoreApplication::exit(0);
}

void Agent::pollTimeout()
{
    // TODO: ... scan the console and write it to the socket.
}
