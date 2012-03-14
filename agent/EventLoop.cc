#include "EventLoop.h"
#include "NamedPipe.h"
#include "../Shared/DebugClient.h"
#include <assert.h>

EventLoop::EventLoop() : m_exiting(false), m_pollInterval(0)
{
}

EventLoop::~EventLoop()
{
    for (size_t i = 0; i < m_pipes.size(); ++i)
        delete m_pipes[i];
}

void EventLoop::run()
{
    std::vector<HANDLE> waitHandles;
    DWORD pollTime = GetTickCount();
    while (!m_exiting) {
        Trace("poll...");
        waitHandles.reserve(m_pipes.size() * 2);
        waitHandles.clear();
        for (size_t i = 0; i < m_pipes.size(); ++i) {
            HANDLE pipe1 = m_pipes[i]->getWaitEvent1();
            HANDLE pipe2 = m_pipes[i]->getWaitEvent2();
            if (pipe1 != NULL)
                waitHandles.push_back(pipe1);
            if (pipe2 != NULL)
                waitHandles.push_back(pipe2);
        }
        DWORD timeout = INFINITE;
        if (m_pollInterval > 0) {
            int elapsed = GetTickCount() - pollTime;
            if (elapsed < m_pollInterval)
                timeout = m_pollInterval - elapsed;
            else
                timeout = 0;
        }
        Trace("poll... timeout is %d ms", (int)timeout);
        DWORD result = WaitForMultipleObjects(waitHandles.size(),
                                              waitHandles.data(),
                                              FALSE,
                                              timeout);
        Trace("poll... result is 0x%x", result);
        assert(result != WAIT_FAILED);
        if (result != WAIT_TIMEOUT) {
            for (size_t i = 0; i < m_pipes.size(); ++i)
                m_pipes[i]->poll();
            onPipeIo();
        }
        if (m_pollInterval > 0) {
            int elapsed = GetTickCount() - pollTime;
            if (elapsed >= m_pollInterval) {
                onPollTimeout();
                pollTime = GetTickCount();
            }
        }
    }
}

NamedPipe *EventLoop::createNamedPipe()
{
    NamedPipe *ret = new NamedPipe();
    m_pipes.push_back(ret);
    return ret;
}

void EventLoop::setPollInterval(int ms)
{
    m_pollInterval = ms;
}

void EventLoop::exit()
{
    m_exiting = true;
}

void EventLoop::onPollTimeout()
{
}

void EventLoop::onPipeIo()
{
}
