#include "Server.h"
#include "Session.h"
#include <QTcpServer>
#include <QTcpSocket>

Server::Server(QObject *parent) :
    QObject(parent)
{
    m_tcpServer = new QTcpServer(this);
    connect(m_tcpServer, SIGNAL(newConnection()), SLOT(newConnection()));
    bool ret = m_tcpServer->listen(QHostAddress::Any, 23);
    Q_ASSERT(ret);
}

void Server::newConnection()
{
    QTcpSocket *socket = m_tcpServer->nextPendingConnection();
    new Session(socket, this);
}
