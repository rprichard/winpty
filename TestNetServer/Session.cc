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

    connect(m_agentClient->getSocket(), SIGNAL(disconnected()), SLOT(cleanup()));
    connect(m_socket, SIGNAL(disconnected()), SLOT(cleanup()));
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
    //Trace("onAgentReadyRead: read [...%s]", data.simplified().right(8).data());
    m_socket->write(data);
}

void Session::cleanup()
{
    Trace("%s entered", __FUNCTION__);
    QAbstractSocket *s1 = m_socket;
    QLocalSocket *s2 = m_agentClient->getSocket();
    if (s1->state() == QAbstractSocket::ConnectedState)
        s1->disconnectFromHost();
    if (s2->state() == QLocalSocket::ConnectedState)
        s2->disconnectFromServer();
    if (s1->state() == QAbstractSocket::UnconnectedState &&
            s2->state() == QLocalSocket::UnconnectedState)
        deleteLater();
    Trace("%s exited", __FUNCTION__);
}
