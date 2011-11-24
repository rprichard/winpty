#ifndef TELNETSERVER_H
#define TELNETSERVER_H

#include <QObject>

class QTcpServer;

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = 0);

signals:

private slots:
    void newConnection();

public slots:

private:
    QTcpServer *m_tcpServer;
};

#endif // TELNETSERVER_H
