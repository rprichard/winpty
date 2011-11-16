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
    explicit Agent(const QString &socketServer,
                   int initialCols, int initialRows,
                   QObject *parent = 0);
    virtual ~Agent();

signals:

private slots:
    void socketReadyRead();
    void socketDisconnected();
    void pollTimeout();

private:
    void scrapeOutput();

private:
    QLocalSocket *m_socket;
    QTimer *m_timer;
    HANDLE m_conin;
    HANDLE m_conout;
};

#endif // AGENT_H
