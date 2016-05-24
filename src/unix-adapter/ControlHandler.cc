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

#include "ControlHandler.h"

#include <assert.h>
#include <errno.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include "../shared/DebugClient.h"
#include "Event.h"
#include "Util.h"
#include "WakeupFd.h"

ControlHandler::ControlHandler(HANDLE control, HANDLE winpty, WakeupFd &completionWakeup) :
    m_control(control),
    m_winpty(winpty),
    m_completionWakeup(completionWakeup),
    m_threadHasBeenJoined(false),
    m_shouldShutdown(0),
    m_threadCompleted(0)
{
    pthread_create(&m_thread, NULL, ControlHandler::threadProcS, this);
}

void ControlHandler::shutdown() {
    startShutdown();
    if (!m_threadHasBeenJoined) {
        int ret = pthread_join(m_thread, NULL);
        assert(ret == 0 && "pthread_join failed");
        m_threadHasBeenJoined = true;
    }
}

void ControlHandler::threadProc() {
    Event ioEvent;
    std::vector<char> buffer(4096);
    while (true) {
        // Handle shutdown
        m_wakeup.reset();
        if (m_shouldShutdown) {
            trace("ControlHandler: shutting down");
            break;
        }

        // Read from the pipe.
        {
            DWORD numRead;
            OVERLAPPED over = {0};
            over.hEvent = ioEvent.handle();
            BOOL ret = ReadFile(m_control,
                                &buffer[0], buffer.size(),
                                &numRead,
                                &over);
            if (!ret && GetLastError() == ERROR_IO_PENDING) {
                const HANDLE handles[] = {
                    ioEvent.handle(),
                    m_wakeup.handle(),
                };
                const DWORD waitRet =
                        WaitForMultipleObjects(2, handles, FALSE, INFINITE);
                if (waitRet == WAIT_OBJECT_0 + 1) {
                    trace("ControlHandler: shutting down, canceling I/O");
                    assert(m_shouldShutdown);
                    CancelIo(m_control);
                    GetOverlappedResult(m_control, &over, &numRead, TRUE);
                    break;
                }
                assert(waitRet == WAIT_OBJECT_0);
                ret = GetOverlappedResult(m_control, &over, &numRead, TRUE);
            }
            if (!ret || numRead == 0) {
                if (!ret && GetLastError() == ERROR_BROKEN_PIPE) {
                    trace("ControlHandler: pipe closed: numRead=%u",
                          static_cast<unsigned int>(numRead));
                } else {
                    trace("ControlHandler: read failed: "
                          "ret=%d lastError=0x%x numRead=%u",
                          ret,
                          static_cast<unsigned int>(GetLastError()),
                          static_cast<unsigned int>(numRead));
                }
                break;
            }
        } //end read
        
        {
            //Write to pipe
            DWORD written;
            OVERLAPPED over = {0};
            over.hEvent = ioEvent.handle();
            BOOL ret = WriteFile(m_winpty,
                                 &buffer[0], numRead,
                                 &written,
                                 &over);
            if (!ret && GetLastError() == ERROR_IO_PENDING) {
                const HANDLE handles[] = {
                    ioEvent.handle(),
                    m_wakeup.handle(),
                };
                const DWORD waitRet =
                        WaitForMultipleObjects(2, handles, FALSE, INFINITE);
                if (waitRet == WAIT_OBJECT_0 + 1) {
                    trace("InputHandler: shutting down, canceling I/O");
                    assert(m_shouldShutdown);
                    CancelIo(m_winpty);
                    GetOverlappedResult(m_winpty, &over, &written, TRUE);
                    break;
                }
                assert(waitRet == WAIT_OBJECT_0);
                ret = GetOverlappedResult(m_winpty, &over, &written, TRUE);
            }
            if (!ret || written != static_cast<DWORD>(numRead)) {
                if (!ret && GetLastError() == ERROR_BROKEN_PIPE) {
                    trace("InputHandler: pipe closed: written=%u",
                          static_cast<unsigned int>(written));
                } else {
                    trace("InputHandler: write failed: "
                          "ret=%d lastError=0x%x numRead=%d written=%u",
                          ret,
                          static_cast<unsigned int>(GetLastError()),
                          numRead,
                          static_cast<unsigned int>(written));
                }
                break;
            }
        }//end write
    }
    m_threadCompleted = 1;
    m_completionWakeup.set();
}
