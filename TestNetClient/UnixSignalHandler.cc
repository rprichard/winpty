// Handles Unix signals in a Qt program.
//

// TODO: Are Qt programs automatically multi-threaded?  I don't think they
// are...  I wonder if this code works OK in a multi-threaded program.

#include "UnixSignalHandler.h"
#include <QSocketNotifier>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

UnixSignalHandler *UnixSignalHandler::handlers[_NSIG];

UnixSignalHandler::UnixSignalHandler(int signum, QObject *parent) :
    QObject(parent), m_signum(signum)
{
    Q_ASSERT(m_signum < _NSIG);

    if (handlers[m_signum] != NULL)
        qFatal("Signal handler for signal %d is already registered.", m_signum);
    handlers[m_signum] = this;

    int pipeFd[2];
    if (pipe2(pipeFd, O_NONBLOCK | O_CLOEXEC) != 0)
        qFatal("Could not create signal %d pipe", m_signum);
    m_pipeReadFd = pipeFd[0];
    m_pipeWriteFd = pipeFd[1];
    m_sn = new QSocketNotifier(m_pipeReadFd, QSocketNotifier::Read, this);
    connect(m_sn, SIGNAL(activated(int)), SLOT(pipeReadActivated()));

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = UnixSignalHandler::signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags |= SA_RESTART;

    if (sigaction(m_signum, &sa, &m_oldsa) != 0)
        qFatal("sigaction for signal %d failed", m_signum);
}

UnixSignalHandler::~UnixSignalHandler()
{
    sigaction(m_signum, &m_oldsa, NULL);
    // TODO: If there is data on the pipe, maybe we could emit the Qt signal
    // here.
    close(m_pipeReadFd);
    close(m_pipeWriteFd);
    handlers[m_signum] = NULL;
}

void UnixSignalHandler::pipeReadActivated()
{
    m_sn->setEnabled(false);
    char dummy;
    read(m_pipeReadFd, &dummy, 1);
    emit signaled(m_signum);
    m_sn->setEnabled(true);
}

void UnixSignalHandler::signalHandler(int signum)
{
    if (handlers[signum] != 0) {
        char dummy = 0;
        write(handlers[signum]->m_pipeWriteFd, &dummy, 1);
    }
}
