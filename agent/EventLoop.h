#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <vector>

class NamedPipe;

class EventLoop
{
public:
    EventLoop();
    virtual ~EventLoop();
    void run();

protected:
    NamedPipe *createNamedPipe();
    void setPollInterval(int ms);
    void shutdown();
    virtual void onPollTimeout();
    virtual void onPipeIo(NamedPipe *namedPipe);

private:
    bool m_exiting;
    std::vector<NamedPipe*> m_pipes;
    int m_pollInterval;
};

#endif // EVENTLOOP_H
