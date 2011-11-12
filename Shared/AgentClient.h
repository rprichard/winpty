#ifndef AGENTCLIENT_H
#define AGENTCLIENT_H

#include <QObject>

class QLocalServer;
class QLocalSocket;
typedef struct _PROCESS_INFORMATION PROCESS_INFORMATION;
typedef struct _INPUT_RECORD INPUT_RECORD;

class AgentClient : public QObject
{
    Q_OBJECT
public:
    explicit AgentClient(QObject *parent = 0);
    //void sendKeyPress(QKeyEvent *event);
    //void sendKeyRelease(QKeyEvent *event);
    int agentPid();
    void writeInputRecord(INPUT_RECORD *ir);
    void startShell();
    QLocalSocket *getSocket();

signals:

public slots:

private:
    QLocalSocket *m_socket;
    PROCESS_INFORMATION *m_agentProcess;
    static int m_counter;
};

#endif // AGENTCLIENT_H
