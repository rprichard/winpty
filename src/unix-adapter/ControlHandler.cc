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

ControlHandler::ControlHandler(HANDLE r, HANDLE w, WakeupFd &completionWakeup) :
    m_read_pipe(r),
    m_write_pipe(w),
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
    while (true) {
        // Handle shutdown
        m_wakeup.reset();
        if (m_shouldShutdown) {
            trace("ControlHandler: shutting down");
            break;
        }

        // Read from the pipe.
        DWORD numRead;
        char data;
            
        BOOL ret = ReadFile(m_read_pipe,
                            &data, 1,
                            &numRead,
                            nullptr);
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
        
        //Write to pipe
        DWORD written;
        ret = WriteFile(m_write_pipe,
                        &data, numRead,
                        &written,
                        nullptr);
        if (!ret || written != numRead) {
            if (!ret && GetLastError() == ERROR_BROKEN_PIPE) {
                trace("ControlHandler: pipe closed: written=%u",
                      static_cast<unsigned int>(written));
            } else {
                trace("ControlHandler: write failed: "
                      "ret=%d lastError=0x%x numRead=%d written=%u",
                      ret,
                      static_cast<unsigned int>(GetLastError()),
                      numRead,
                      static_cast<unsigned int>(written));
            }
            break;
        }
        
    }
    m_threadCompleted = 1;
    m_completionWakeup.set();
}
