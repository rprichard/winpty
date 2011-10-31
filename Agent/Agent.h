#ifndef AGENT_H
#define AGENT_H

#include <QObject>
#include <windows.h>

class QLocalSocket;
class QTimer;

class Agent : public QObject
{
    Q_OBJECT
public:
    explicit Agent(const QString &socketServer, QObject *parent = 0);

signals:

private slots:
    void socketReadyRead();
    void socketClosed();
    void pollTimeout();

private:
    QLocalSocket *m_socket;
    QTimer *m_timer;
    HANDLE m_conin;
    HANDLE m_conout;
};

#endif // AGENT_H
