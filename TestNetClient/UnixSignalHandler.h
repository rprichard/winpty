#ifndef UNIXSIGNALHANDLER_H
#define UNIXSIGNALHANDLER_H

#include <QObject>
#include <signal.h>

class QSocketNotifier;

class UnixSignalHandler : public QObject
{
    Q_OBJECT
public:
    explicit UnixSignalHandler(int signum, QObject *parent = 0);
    ~UnixSignalHandler();

signals:
    void signaled(int signum);

public slots:

private slots:
    void pipeReadActivated();

private:
    static void signalHandler(int signum);

    static UnixSignalHandler *handlers[_NSIG];
    QSocketNotifier *m_sn;
    int m_signum;
    int m_pipeReadFd;
    int m_pipeWriteFd;
    struct sigaction m_oldsa;
};

#endif // UNIXSIGNALHANDLER_H
