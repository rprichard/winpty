// Copyright (c) 2011-2015 Ryan Prichard
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

#ifndef UNIX_ADAPTER_CONTROL_INPUT_HANDLER_H
#define UNIX_ADAPTER_CONTROL_INPUT_HANDLER_H

#include <windows.h>
#include <pthread.h>
#include <signal.h>

#include "Event.h"
#include "WakeupFd.h"

// Connect winpty overlapped I/O to Cygwin blocking STDOUT_FILENO.
class ControlInputHandler {
public:
    ControlInputHandler(HANDLE control, HANDLE winpty, WakeupFd &completionWakeup);
    ~ControlInputHandler() { shutdown(); }
    bool isComplete() { return m_threadCompleted; }
    void startShutdown() { m_shouldShutdown = 1; m_wakeup.set(); }
    void shutdown();

private:
    static void *threadProcS(void *pvthis) {
        reinterpret_cast<ControlInputHandler*>(pvthis)->threadProc();
        return NULL;
    }
    void threadProc();

    HANDLE m_control;
    HANDLE m_winpty;
    pthread_t m_thread;
    WakeupFd &m_completionWakeup;
    Event m_wakeup;
    bool m_threadHasBeenJoined;
    volatile sig_atomic_t m_shouldShutdown;
    volatile sig_atomic_t m_threadCompleted;
};

#endif // UNIX_ADAPTER_CONTROL_HANDLER_H
