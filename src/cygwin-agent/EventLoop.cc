// Copyright (c) 2011-2012 Ryan Prichard
// Copyright (c) 2019 Lucio Andrés Illanes Albornoz <lucio@lucioillanes.de>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "EventLoop.h"

#include "NamedPipe.h"
#include "../shared/DebugClient.h"
#include "../shared/WinptyAssert.h"

EventLoop::~EventLoop() {
    for (NamedPipe *pipe : m_pipes) {
        delete pipe;
    }
    m_pipes.clear();
}

NamedPipe &EventLoop::createNamedPipe()
{
    NamedPipe *ret = new NamedPipe();
    m_pipes.push_back(ret);
    return *ret;
}

// Enter the event loop.  Runs until the I/O or timeout handler calls exit().
void EventLoop::run()
{
    bool didSomething = false;
    HANDLE hEvent;
    std::vector<HANDLE> waitHandles;
    while (!m_exiting) {
        didSomething = false;

        // Attempt to make progress with the pipes.
        waitHandles.clear();
        for (size_t i = 0; i < m_pipes.size(); ++i) {
            if (m_pipes[i]->serviceIo(&waitHandles)) {
                onPipeIo(*m_pipes[i]);
                didSomething = true;
            }
        }

        if (didSomething)
            continue;

        // Attempt to make progress with the pty.
        didSomething = onPtyIo();
        if ((hEvent = getPtyEventHandle())) {
            waitHandles.insert(waitHandles.end(), hEvent);
        }

        if (didSomething)
            continue;

        // If there's nothing to do, wait.
        DWORD timeout = INFINITE;
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

    // Attempt to flush pipe queues.
    do {
        didSomething = false;
        for (size_t i = 0; i < m_pipes.size(); ++i) {
            if (m_pipes[i]->serviceIo(&waitHandles)) {
                onPipeIo(*m_pipes[i]);
                didSomething = true;
            }
        }
    } while (didSomething);
}

void EventLoop::shutdown()
{
    m_exiting = true;
}
