#include "Session.h"
#include <QTcpSocket>
#include <QLocalSocket>
#include <QtDebug>
#include "../Shared/AgentClient.h"
#include "../Shared/DebugClient.h"
#include <windows.h>

Session::Session(QTcpSocket *socket, QObject *parent) :
    QObject(parent), m_socket(socket)
{
    m_agentClient = new AgentClient(this);
    connect(m_socket, SIGNAL(readyRead()), SLOT(onSocketReadyRead()));
    m_agentClient->startShell();
    connect(m_agentClient->getSocket(), SIGNAL(readyRead()), SLOT(onAgentReadyRead()));
}

void Session::onSocketReadyRead()
{
    QByteArray data = m_socket->readAll();

    Trace("session: read %d bytes", data.length());

    for (int i = 0; i < data.size(); ++i) {
        Trace("input: %02x", (unsigned char)data[i]);

        short vk = VkKeyScan((unsigned char)data[i]);
        if (vk != -1) {
            INPUT_RECORD ir;
            memset(&ir, 0, sizeof(ir));
            ir.EventType = KEY_EVENT;
            ir.Event.KeyEvent.bKeyDown = TRUE;
            ir.Event.KeyEvent.wVirtualKeyCode = vk & 0xff;
            ir.Event.KeyEvent.wVirtualScanCode = 0;
            ir.Event.KeyEvent.uChar.AsciiChar =  data[i];
            ir.Event.KeyEvent.wRepeatCount = 1;
            m_agentClient->writeInputRecord(&ir);
        }
    }

    Trace("session: completed read", data.length());
}

void Session::onAgentReadyRead()
{
    QByteArray data = m_agentClient->getSocket()->readAll();
    m_socket->write(data);
}
