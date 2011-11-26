#include "Session.h"
#include <QTcpSocket>
#include <QLocalSocket>
#include <QtDebug>
#include "../Shared/AgentClient.h"
#include "../Shared/AgentMsg.h"
#include "../Shared/DebugClient.h"
#include <windows.h>

Session::Session(QTcpSocket *socket, QObject *parent) :
    QObject(parent), m_socket(socket), m_agentClient(NULL)
{
    connect(m_socket, SIGNAL(readyRead()), SLOT(onSocketReadyRead()));
    connect(m_socket, SIGNAL(disconnected()), SLOT(cleanup()));
}

void Session::initializeAgent(int cols, int rows)
{
    Q_ASSERT(m_agentClient == NULL);
    m_agentClient = new AgentClient(cols, rows, this);
    connect(m_agentClient->getSocket(), SIGNAL(readyRead()), SLOT(onAgentReadyRead()));
    connect(m_agentClient->getSocket(), SIGNAL(disconnected()), SLOT(cleanup()));
    m_agentClient->startShell();
}

void Session::onSocketReadyRead()
{
    QByteArray data = m_socket->readAll();

    Trace("session: read %d bytes", data.length());

    for (int i = 0; i < data.size(); ) {
        const int remaining = data.size() - i;

        if (remaining >= 12 && !strncmp(data.constData(), "\x1B[:r", 4)) {
            // Terminal resize.
            char buf[9];
            memcpy(buf, data.constData() + i + 4, 8);
            i += 12;
            buf[8] = '\0';
            unsigned int dim = strtol(buf, NULL, 16);
            AgentMsg msg;
            memset(&msg, 0, sizeof(msg));
            msg.type = AgentMsg::WindowSize;
            msg.u.windowSize.cols = dim & 0xffff;
            msg.u.windowSize.rows = dim >> 16;
            Trace("resize: %d x %d", msg.u.windowSize.cols, msg.u.windowSize.rows);
            if (m_agentClient == NULL) {
                initializeAgent(msg.u.windowSize.cols, msg.u.windowSize.rows);
            } else {
                m_agentClient->writeMsg(msg);
            }
            continue;
        }

        const unsigned char ch = data[i++];
        const short vk = VkKeyScan(ch);
        if (vk != -1) {
            Trace("input: %02x", ch);
            AgentMsg msg;
            memset(&msg, 0, sizeof(msg));
            msg.type = AgentMsg::InputRecord;
            INPUT_RECORD &ir = msg.u.inputRecord;
            ir.EventType = KEY_EVENT;
            ir.Event.KeyEvent.bKeyDown = TRUE;
            ir.Event.KeyEvent.wVirtualKeyCode = vk & 0xff;
            ir.Event.KeyEvent.wVirtualScanCode = 0;
            ir.Event.KeyEvent.uChar.AsciiChar = ch;
            ir.Event.KeyEvent.wRepeatCount = 1;
            if (m_agentClient != NULL)
                m_agentClient->writeMsg(msg);
            continue;
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
