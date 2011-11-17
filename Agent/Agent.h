#ifndef AGENT_H
#define AGENT_H

#include <QObject>
#include <windows.h>

class Win32Console;
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
    void resizeWindow(int cols, int rows);
    void scrapeOutput();

private:
    Win32Console *m_console;
    QLocalSocket *m_socket;
    QTimer *m_timer;
};

#endif // AGENT_H
