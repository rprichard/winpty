// Copyright (c) 2011-2015 Ryan Prichard
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

#ifndef AGENT_CYGWIN_PTY_H
#define AGENT_CYGWIN_PTY_H

#include <windows.h>

#include <termios.h>

#include <atomic>
#include <tuple>
#include <vector>

#include "../shared/OwnedHandle.h"
#include "EventLoop.h"

class EventLoop;

class AgentCygwinPty
{
private:
    friend class EventLoop;

    class IoWorker
    {
    protected:
        static DWORD WINAPI ThreadProc(LPVOID lpParameter);
        virtual DWORD threadProc() = 0;

        HANDLE m_hThread = nullptr;
        AgentCygwinPty &m_pty;

    public:
        IoWorker(AgentCygwinPty &pty);
        ~IoWorker();
        bool cancelWorker();
        virtual bool startWorker();

        CRITICAL_SECTION m_cs;
        OwnedHandle m_event;
        std::vector<std::tuple<void *, size_t, size_t>> m_queue;
        std::atomic<int> m_status;
    };

    class InputWorker : public IoWorker
    {
    protected:
        DWORD threadProc();
    public:
        InputWorker(AgentCygwinPty &pty) : IoWorker(pty) {}
    };

    class OutputWorker : public IoWorker
    {
    private:
        int m_fd_pipe[2] = {-1, -1};
    protected:
        DWORD threadProc();
    public:
        OutputWorker(AgentCygwinPty &pty) : IoWorker(pty) {}
        bool startWorker();
        bool wakeWorker();
    };

public:
    AgentCygwinPty() : m_inputWorker(*this), m_outputWorker(*this) {}
    ~AgentCygwinPty() {close();}

    void close();
    int fork(const std::wstring cmdline, struct winsize pty_winp, HANDLE *phProcess, HANDLE *phThread);
    bool read(const char **pbuf, size_t *pbuf_len);
    bool write(const char *buf, size_t buf_len);

    int m_fd = -1;
    InputWorker m_inputWorker;
    OutputWorker m_outputWorker;
    pid_t m_pid = -1;
};

#endif // AGENT_CYGWIN_PTY_H
