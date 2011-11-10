#ifndef SESSION_H
#define SESSION_H

#include <QObject>

class QTcpSocket;
class AgentClient;

class Session : public QObject
{
    Q_OBJECT
public:
    explicit Session(QTcpSocket *socket, QObject *parent = 0);

signals:

public slots:

private slots:
    void onSocketReadyRead();

private:
    QTcpSocket *m_socket;
    AgentClient *m_agentClient;
};

#endif // SESSION_H
