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

#define _GNU_SOURCE // for ptsname(3)
#include "AgentCygwinPty.h"

#include <pty.h>
#include <unistd.h>
#include <utmp.h>
#include <sys/cygwin.h>

#include "../shared/DebugClient.h"

static OwnedHandle createEvent();
static void ptyForkChildRoutine(const std::wstring cmdline);
static bool ptyForkParentRoutine(int pty_fd, pid_t pty_pid, HANDLE *phProcess, HANDLE *phThread);
static char *wcsToMbs(const wchar_t *text);

// manual reset, initially unset
static OwnedHandle createEvent() {
    HANDLE ret = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    ASSERT(ret != nullptr && "CreateEventW failed");
    return OwnedHandle(ret);
}

// (based on mintty/src/child.c:child_create())
static void ptyForkChildRoutine(const std::wstring cmdline)
{
    struct termios attr;
    int exec_argcw; wchar_t **exec_argvw = NULL;
    const char **exec_argv = NULL;
    int rc;

    // Reset signals; mimick login's behavior by disabling the job control signals
    for (auto signo : {SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGCHLD}) {
        signal(signo, SIG_DFL);
    }
    for (auto signo : {SIGTSTP, SIGTTIN, SIGTTOU}) {
        signal(signo, SIG_IGN);
    }

    // Terminal line settings
    tcgetattr(0, &attr);
    attr.c_cc[VERASE] = CTRL('H');
    attr.c_iflag |= IMAXBEL | IUTF8 | IXANY;
    attr.c_lflag |= ECHOCTL | ECHOE | ECHOK | ECHOKE;
    tcsetattr(0, TCSANOW, &attr);

    // Build exec_argv & invoke command
    if ((exec_argvw = CommandLineToArgvW(cmdline.c_str(), &exec_argcw))) {
        exec_argv = new const char *[exec_argcw + 1]();
        for (int exec_argc = 0; exec_argc < exec_argcw; exec_argc++) {
            exec_argv[exec_argc] = wcsToMbs(exec_argvw[exec_argc]);
            TRACE("exec_argv[%d]=%s", exec_argc, exec_argv[exec_argc]);
        }
        LocalFree(exec_argvw);
        execvp(exec_argv[0], (char* const*)exec_argv);
        TRACE("execvp() failed, errno=%d", errno);
        fflush(stderr), rc = 126;
    } else {
        TRACE("CommandLineToArgvW() returned NULL, GetLastError()=%d", GetLastError());
        rc = 127;
    }

    for (int exec_argc = 0; exec_argc < exec_argcw; exec_argc++) {
        delete exec_argv[exec_argc];
    }
    delete exec_argv;
    exit(rc);
}

// (based on mintty/src/child.c:child_create())
static bool ptyForkParentRoutine(int pty_fd, pid_t pty_pid, HANDLE *phProcess, HANDLE *phThread)
{
    const char *dev, *login_name;
    HANDLE hProcess = NULL, hThread = NULL;
    bool rc = false;
    struct utmp ut;
    int winpid;

    winpid = cygwin_internal(CW_CYGWIN_PID_TO_WINPID, pty_pid);
    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, winpid);
    hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, winpid);
    if ((dev = ptsname(pty_fd))) {
        memset(&ut, 0, sizeof(ut));
        if (strncmp(dev, "/dev/", 5) == 0) {
            dev += 5;
        }
        strlcpy(ut.ut_line, dev, sizeof(ut.ut_line));

        if (strncmp(&dev[1], "ty", 2) == 0) {
            dev += 1 + 2;
        }
        else if (strncmp(dev, "pts/", 4) == 0) {
            dev += 4;
        }
        strncpy(ut.ut_id, dev, sizeof(ut.ut_id));

        gethostname(ut.ut_host, sizeof(ut.ut_host));
        ut.ut_pid = pty_pid, ut.ut_time = time(0), ut.ut_type = USER_PROCESS;
        if ((login_name = getlogin())) {
            strlcpy(ut.ut_user, login_name ? login_name : "?", sizeof(ut.ut_user));
            login(&ut), rc = true;
        } else {
            TRACE("getlogin() failed, errno=%d", errno);
        }
    } else {
        TRACE("ptsname() failed, errno=%d", errno);
    }
    if (rc) {
        *phProcess = hProcess, *phThread = hThread;
    }
    return rc;
}

static char *wcsToMbs(const wchar_t *text)
{
    // Calling wcstombs with a NULL first argument seems to be broken on MSYS.
    // Instead of returning the size of the converted string, it returns 0.
    // Using wcslen(text) * 3 is big enough for UTF-8 and probably other
    // encodings.  For UTF-8, codepoints that fit in a single wchar
    // (U+0000 to U+FFFF) are encoded using 1-3 bytes.  The remaining code
    // points needs two wchar's and are encoded using 4 bytes.
    size_t maxLen = wcslen(text) * 3 + 1;
    char *ret = new char[maxLen];
    size_t len = wcstombs(ret, text, maxLen);
    if (len == (size_t)-1 || len >= maxLen) {
        delete [] ret;
        return NULL;
    } else {
        return ret;
    }
}

void AgentCygwinPty::close()
{
    if (m_fd != -1) {
        ::close(m_fd), m_fd = -1;
    }
    m_inputWorker.cancelWorker(), m_outputWorker.cancelWorker();
    if (m_pid != -1) {
        kill(m_pid, SIGTERM), kill(m_pid, SIGKILL), m_pid = -1;
    }
}

int AgentCygwinPty::fork(const std::wstring cmdline, struct winsize pty_winp, HANDLE *phProcess, HANDLE *phThread)
{
    HANDLE hProcess, hThread;
    int pty_fd; pid_t pty_pid;
    int status = 0;

    switch ((pty_pid = forkpty(&pty_fd, 0, 0, &pty_winp))) {
    case -1: status = errno; break;
    case  0: ptyForkChildRoutine(cmdline); break;
    default: ptyForkParentRoutine(pty_fd, pty_pid, &hProcess, &hThread);
    }
    if (status == 0) {
        *phProcess = hProcess, *phThread = hThread;
        m_fd = pty_fd, m_pid = pty_pid;
        if (!m_inputWorker.startWorker() || !m_outputWorker.startWorker()) {
            close(), status = EINVAL;
        }
    }
    return status;
}

bool AgentCygwinPty::read(const char **pbuf, size_t *pbuf_len)
{
    std::tuple<void *, size_t, size_t> item;
    bool itemfl = false;

    EnterCriticalSection(&m_inputWorker.m_cs);
    if (m_inputWorker.m_queue.size() > 0) {
        item = m_inputWorker.m_queue[0], itemfl = true;
        m_inputWorker.m_queue.erase(m_inputWorker.m_queue.begin());
        if (m_inputWorker.m_queue.size() == 0) {
            ResetEvent(m_inputWorker.m_event.get());
        }
    }
    LeaveCriticalSection(&m_inputWorker.m_cs);
    if (itemfl) {
        *pbuf = (const char *)std::get<0>(item), *pbuf_len = std::get<1>(item);
    } else {
        *pbuf = nullptr, *pbuf_len = 0;
    }
    return (m_inputWorker.m_status == 0);
}

bool AgentCygwinPty::write(const char *buf, size_t buf_len)
{
    if (buf_len > 0) {
        EnterCriticalSection(&m_outputWorker.m_cs);
        m_outputWorker.m_queue.push_back(std::tuple<void *, size_t, size_t>((void *)buf, buf_len, 0));
        m_outputWorker.wakeWorker();
        LeaveCriticalSection(&m_outputWorker.m_cs);
        return (m_outputWorker.m_status == 0);
    } else {
        return false;
    }
}

AgentCygwinPty::IoWorker::IoWorker(AgentCygwinPty &pty) :
    m_pty(pty), m_event(createEvent()), m_status(0)
{
    InitializeCriticalSection(&m_cs); ResetEvent(m_event.get());
}

AgentCygwinPty::IoWorker::~IoWorker()
{
    cancelWorker();
    for (auto &item : m_queue) {
        if (std::get<0>(item)) {
            delete (char *)std::get<0>(item);
        }
    }
}

bool AgentCygwinPty::IoWorker::cancelWorker()
{
    if (m_hThread) {
        EnterCriticalSection(&m_cs);
        TerminateThread(m_hThread, 0);
        LeaveCriticalSection(&m_cs);
        return true;
    } else {
        return false;
    }
}

bool AgentCygwinPty::IoWorker::startWorker() {
    if (!(m_hThread = CreateThread(NULL, 0, ThreadProc, this, 0, NULL))) {
        TRACE("CreateThread() returned nullptr");
        return false;
    } else {
        return true;
    }
}

DWORD WINAPI AgentCygwinPty::IoWorker::ThreadProc(LPVOID lpParameter)
{
    AgentCygwinPty::IoWorker *ioWorker = (AgentCygwinPty::IoWorker *)lpParameter;
    return ioWorker->threadProc();
}

DWORD AgentCygwinPty::InputWorker::threadProc()
{
    static char buf[4096];
    char *item_buf;
    ssize_t nread;
    int status = 0;

    while (status == 0) {
        //
        // (based on mintty/src/child.c:child_proc())
        // Pty devices on old Cygwin versions (pre 1005) deliver only 4 bytes
        // at a time, and newer ones or MSYS2 deliver up to 256 at a time.
        // so call read() repeatedly until we have a worthwhile haul.
        // this avoids most partial updates, results in less flickering/tearing.
        //
        nread = ::read(m_pty.m_fd, buf, sizeof(buf));
        TRACE("InputWorker::threadProc(): read %ld bytes", nread);
        if (nread == 0) {
            status = EPIPE;
        } else if (nread < 0) {
            status = errno;
        } else {
            item_buf = new char[nread];
            memcpy(item_buf, buf, nread);
            EnterCriticalSection(&m_cs);
            m_queue.push_back(std::tuple<void *, size_t, size_t>(item_buf, nread, 0));
            SetEvent(m_event.get());
            LeaveCriticalSection(&m_cs);
        }
    }
    m_status = status;
    TRACE("InputWorker::threadProc(): exiting w/ status=%d", status);
    SetEvent(m_event.get());
    return 0;
}

bool AgentCygwinPty::OutputWorker::startWorker()
{
    if (pipe(m_fd_pipe) < 0) {
        TRACE("pipe() returned -1");
        return false;
    } else if (!AgentCygwinPty::IoWorker::startWorker()) {
        ::close(m_fd_pipe[0]); ::close(m_fd_pipe[1]); m_fd_pipe[0] = m_fd_pipe[1] = -1;
        return false;
    } else {
        return true;
    }
}

DWORD AgentCygwinPty::OutputWorker::threadProc()
{
    std::tuple<void *, size_t, size_t> item;
    ssize_t nread, nwritten;
    char pipe_buf[1];
    int status = 0;

    while (status == 0) {
        if ((nread = ::read(m_fd_pipe[0], pipe_buf, sizeof(pipe_buf))) <= 0) {
            status = (nread == 0) ? EPIPE : errno;
        } else {
            EnterCriticalSection(&m_cs);
            if (m_queue.size() > 0) {
                item = m_queue[0];
                nwritten = ::write(m_pty.m_fd, (uint8_t *)std::get<0>(item) + std::get<2>(item), std::get<1>(item) - std::get<2>(item));
                TRACE("OutputWorker::threadProc(): wrote %ld bytes", nwritten);
                if ((nwritten > 0) && ((size_t)nwritten == (std::get<1>(item) + std::get<2>(item)))) {
                    m_queue.erase(m_queue.begin());
                } else if ((nwritten > 0) && ((size_t)nwritten < (std::get<1>(item) + std::get<2>(item)))) {
                    std::get<2>(item) += nwritten;
                } else if (nwritten < 0) {
                    status = errno;
                }
            }
            LeaveCriticalSection(&m_cs);
        }
    }
    m_status = status;
    TRACE("OutputWorker::threadProc(): exiting w/ status=%d", status);
    return 0;
}

bool AgentCygwinPty::OutputWorker::wakeWorker()
{
    bool rc = false;

    if (m_fd_pipe[1] != -1) {
        rc = ::write(m_fd_pipe[1], "", 1) == 1;
    }
    return rc;
}
