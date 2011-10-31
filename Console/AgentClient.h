#ifndef AGENTCLIENT_H
#define AGENTCLIENT_H

#include <QObject>
#include <QKeyEvent>

class QLocalServer;
class QLocalSocket;
typedef struct _PROCESS_INFORMATION PROCESS_INFORMATION;

class AgentClient : public QObject
{
    Q_OBJECT
public:
    explicit AgentClient(QObject *parent = 0);
    void sendKeyPress(QKeyEvent *event);
    void sendKeyRelease(QKeyEvent *event);
    int agentPid();

signals:

public slots:

private:
    QLocalServer *m_socketServer;
    QLocalSocket *m_socket;
    PROCESS_INFORMATION *m_agentProcess;
    static int m_counter;
};

#endif // AGENTCLIENT_H
