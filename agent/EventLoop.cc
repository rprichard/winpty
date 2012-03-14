#include "EventLoop.h"
#include "NamedPipe.h"
#include "AgentAssert.h"
#include "../Shared/DebugClient.h"

EventLoop::EventLoop() : m_exiting(false), m_pollInterval(0)
{
}

EventLoop::~EventLoop()
{
    for (size_t i = 0; i < m_pipes.size(); ++i)
        delete m_pipes[i];
}

// Enter the event loop.  Runs until the I/O or timeout handler calls exit().
void EventLoop::run()
{
    std::vector<HANDLE> waitHandles;
    DWORD lastTime = GetTickCount();
    while (!m_exiting) {
        bool didSomething = false;

        // Attempt to make progress with the pipes.
        waitHandles.clear();
        for (size_t i = 0; i < m_pipes.size(); ++i) {
            if (m_pipes[i]->serviceIo(&waitHandles)) {
                onPipeIo(m_pipes[i]);
                didSomething = true;
            }
        }

        // Call the timeout if enough time has elapsed.
        if (m_pollInterval > 0) {
            int elapsed = GetTickCount() - lastTime;
            if (elapsed >= m_pollInterval) {
                onPollTimeout();
                lastTime = GetTickCount();
                didSomething = true;
            }
        }

        if (didSomething)
            continue;

        // If there's nothing to do, wait.
        DWORD timeout = INFINITE;
        if (m_pollInterval > 0)
            timeout = std::max(0, (int)(lastTime + m_pollInterval - GetTickCount()));
        if (waitHandles.size() == 0) {
            ASSERT(timeout != INFINITE);
            if (timeout > 0)
                Sleep(timeout);
        } else {
            DWORD result = WaitForMultipleObjects(waitHandles.size(),
                                                  waitHandles.data(),
                                                  FALSE,
                                                  timeout);
            ASSERT(result != WAIT_FAILED);
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

void EventLoop::shutdown()
{
    m_exiting = true;
}

void EventLoop::onPollTimeout()
{
}

void EventLoop::onPipeIo(NamedPipe *namedPipe)
{
}
