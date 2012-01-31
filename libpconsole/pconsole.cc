#include <pconsole.h>
#include <windows.h>
#include <assert.h>
#include <string.h>
#include <string>
#include <vector>
#include <sstream>
#include "../Shared/DebugClient.h"

// TODO: Error handling, handle out-of-memory.

#define AGENT_EXE L"pconsole-agent.exe"

static volatile LONG consoleCounter;
const int bufSize = 4096;

static WINAPI DWORD serviceThread(void *threadParam);

struct pconsole_s {
    pconsole_s();
    HANDLE dataPipe;
    int agentPid;

    char dataWriteBuffer[bufSize];
    int dataWriteAmount;
    char dataReadBuffer[bufSize];
    int dataReadStart;
    int dataReadAmount;
    OVERLAPPED dataReadOver;
    OVERLAPPED dataWriteOver;
    HANDLE dataReadEvent;
    HANDLE dataWriteEvent;
    bool dataReadPending;
    bool dataWritePending;

    DWORD serviceThreadId;
    HANDLE ioEvent;
    bool ioCallbackFlag;
    pconsole_io_cb ioCallback;

    CRITICAL_SECTION lock;
};

pconsole_s::pconsole_s() : dataPipe(NULL), agentPid(-1)
{
}

static HMODULE getCurrentModule()
{
    HMODULE module;
    if (!GetModuleHandleEx(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCTSTR)getCurrentModule,
                &module)) {
        assert(false);
    }
    return module;
}

static std::wstring getModuleFileName(HMODULE module)
{
    const int bufsize = 4096;
    wchar_t path[bufsize];
    int size = GetModuleFileName(module, path, bufsize);
    assert(size != 0 && size != bufsize);
    return std::wstring(path);
}

static std::wstring dirname(const std::wstring &path)
{
    std::wstring::size_type pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos)
        return L"";
    else
        return path.substr(0, pos);
}

static std::wstring basename(const std::wstring &path)
{
    std::wstring::size_type pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos)
        return path;
    else
        return path.substr(pos + 1);
}

static bool pathExists(const std::wstring &path)
{
    return GetFileAttributes(path.c_str()) != 0xFFFFFFFF;
}

PCONSOLE_API
pconsole_s *pconsole_open(int cols, int rows)
{
    BOOL success;

    pconsole_s *pconsole = new pconsole_s;

    // Look for the Agent executable.
    std::wstring progDir = dirname(getModuleFileName(getCurrentModule()));
    std::wstring agentProgram;
    if (pathExists(progDir + L"\\"AGENT_EXE)) {
        agentProgram = progDir + L"\\"AGENT_EXE;
    } else {
        // The development directory structure looks like this:
        //     root/
        //         agent/
        //             pconsole-agent.exe
        //         libpconsole/
        //             pconsole.dll
        agentProgram = dirname(progDir) + L"\\agent\\"AGENT_EXE;
        if (!pathExists(agentProgram)) {
            assert(false);
        }
    }

    // Start a named pipe server.
    std::wstringstream serverNameStream;
    serverNameStream << L"\\\\.\\pipe\\pconsole-" << GetCurrentProcessId()
                     << L"-" << InterlockedIncrement(&consoleCounter);
    std::wstring serverName = serverNameStream.str();
    pconsole->dataPipe = CreateNamedPipe(serverName.c_str(),
                    /*dwOpenMode=*/PIPE_ACCESS_DUPLEX |
                                        FILE_FLAG_FIRST_PIPE_INSTANCE |
                                        FILE_FLAG_OVERLAPPED,
                    /*dwPipeMode=*/0,
                    /*nMaxInstances=*/1,
                    /*nOutBufferSize=*/0,
                    /*nInBufferSize=*/0,
                    /*nDefaultTimeOut=*/3000,
                    NULL);
    if (pconsole->dataPipe == INVALID_HANDLE_VALUE)
        return NULL;

    std::wstringstream agentCmdLineStream;
    agentCmdLineStream << L"\"" << agentProgram << L"\" "
                       << serverName << " "
                       << cols << " " << rows;
    std::wstring agentCmdLine = agentCmdLineStream.str();

    Trace("Starting agent");
    //Trace("Starting Agent: [%s]", agentCmdLine.toStdString().c_str());

    // Get a non-interactive window station for the agent.
    // TODO: review security w.r.t. windowstation and desktop.
    HWINSTA originalStation = GetProcessWindowStation();
    HWINSTA station = CreateWindowStation(NULL, 0, WINSTA_ALL_ACCESS, NULL);
    success = SetProcessWindowStation(station);
    assert(success);
    HDESK desktop = CreateDesktop(L"Default", NULL, NULL, 0, GENERIC_ALL, NULL);
    assert(originalStation != NULL);
    assert(station != NULL);
    assert(desktop != NULL);
    wchar_t stationNameWStr[256];
    success = GetUserObjectInformation(station, UOI_NAME,
                                       stationNameWStr, sizeof(stationNameWStr),
                                       NULL);
    assert(success);
    std::wstring startupDesktop = std::wstring(stationNameWStr) + L"\\Default";

    // Start the agent.
    STARTUPINFO sui;
    memset(&sui, 0, sizeof(sui));
    sui.cb = sizeof(sui);
    // TODO: Put this back.
    sui.lpDesktop = (LPWSTR)startupDesktop.c_str();
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    std::vector<wchar_t> cmdline(agentCmdLine.size() + 1);
    agentCmdLine.copy(&cmdline[0], agentCmdLine.size());
    cmdline[agentCmdLine.size()] = L'\0';
    success = CreateProcess(
        agentProgram.c_str(),
        &cmdline[0],
        NULL, NULL,
        /*bInheritHandles=*/FALSE,
        /*dwCreationFlags=*/CREATE_NEW_CONSOLE,
        NULL, NULL,
        &sui, &pi);
    if (!success) {
        // qFatal("Could not start agent subprocess.");
        assert(false);
    }
    pconsole->agentPid = pi.dwProcessId;
    //qDebug("New child process: PID %d", (int)m_agentProcess->dwProcessId);

    // Connect the named pipe.
    OVERLAPPED over;
    memset(&over, 0, sizeof(over));
    over.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    assert(over.hEvent != NULL);
    success = ConnectNamedPipe(pconsole->dataPipe, &over);
    if (!success && GetLastError() == ERROR_IO_PENDING) {
        DWORD actual;
        success = GetOverlappedResult(pconsole->dataPipe, &over, &actual, TRUE);
    }
    if (!success && GetLastError() == ERROR_PIPE_CONNECTED)
        success = TRUE;
    assert(success);

    // TODO: Review security w.r.t. the named pipe.  Ensure that we're really
    // connected to the agent we just started.  (e.g. Block network
    // connections, set an ACL, call GetNamedPipeClientProcessId, etc.
    // Alternatively, connect the client end of the named pipe and pass an
    // inheritable handle to the agent instead.)  It might be a good idea to
    // run the agent (and therefore the Win7 conhost) under the logged in user
    // for an SSH connection.

    // TODO: error handling?
    CloseHandle(over.hEvent);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    SetProcessWindowStation(originalStation);
    CloseDesktop(desktop);
    CloseWindowStation(station);

    // Create events.
    pconsole->ioEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    pconsole->dataReadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    pconsole->dataWriteEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    InitializeCriticalSection(&pconsole->lock);

    CreateThread(NULL, 0, serviceThread, pconsole, 0, &pconsole->serviceThreadId);

    return pconsole;
}

PCONSOLE_API void pconsole_set_io_cb(pconsole_t *pc, pconsole_io_cb cb)
{
    EnterCriticalSection(&pc->lock);
    pc->ioCallback = cb;
    LeaveCriticalSection(&pc->lock);
}

/*
PCONSOLE_API void pconsole_set_process_exit_cb(pconsole_t *pconsole,
					       pconsole_process_exit_cb cb)
{
    // TODO: implement
}
*/

PCONSOLE_API int pconsole_start_process(pconsole_t *pconsole,
					const wchar_t *program,
					const wchar_t *cmdline,
					const wchar_t *cwd,
					const wchar_t *const *env)
{
#if 0
    int ret = -1;

    if (!FreeConsole())
        Trace("FreeConsole failed");
    if (!AttachConsole(console->agentPid))
        Trace("AttachConsole to pid %d failed", console->agentPid);

    HANDLE conout1, conout2, conin;

    {
        // TODO: Should the permissions be more restrictive?
        // TODO: Is this code necessary or even desirable?  If I
        // don't change these handles, then are the old values
        // invalid?  If so, what happens if I create a console
        // subprocess?
        // The handle must be inheritable.  See comment below.
        SECURITY_ATTRIBUTES sa;
        memset(&sa, 0, sizeof(sa));
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        conout1 = CreateFile(L"CONOUT$",
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa,
            OPEN_EXISTING,
            0, NULL);
        conout2 = CreateFile(L"CONOUT$",
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa,
            OPEN_EXISTING,
            0, NULL);
        conin = CreateFile(L"CONIN$",
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa,
            OPEN_EXISTING,
            0, NULL);
        assert(conin != NULL);
        assert(conout1 != NULL);
        assert(conout2 != NULL);
        /*
        BOOL success;
        success = SetStdHandle(STD_OUTPUT_HANDLE, conout1);
        Q_ASSERT(success);
        success = SetStdHandle(STD_ERROR_HANDLE, conout2);
        Q_ASSERT(success);
        success = SetStdHandle(STD_INPUT_HANDLE, conin);
        Q_ASSERT(success);
        */
    }

    {
        wchar_t *cmdlineCopy = NULL;
        if (cmdline != NULL) {
            cmdlineCopy = new wchar_t[wcslen(cmdline) + 1];
            wcscpy(cmdlineCopy, cmdline);
        }
        STARTUPINFO sui;
        memset(&sui, 0, sizeof(sui));
        sui.cb = sizeof(sui);
        sui.dwFlags = STARTF_USESTDHANDLES;
        sui.hStdInput = conin;
        sui.hStdOutput = conout1;
        sui.hStdError = conout2;
        PROCESS_INFORMATION pi;
        memset(&pi, 0, sizeof(pi));
        BOOL success = CreateProcess(
                    program,
                    cmdlineCopy,
                    NULL,
                    NULL,
                    FALSE,
                    0,
                    NULL,
                    cwd,
                    &sui,
                    &pi);
        delete [] cmdlineCopy;
        if (success) {
            ret = pi.dwProcessId;
            Trace("Started shell pid %d", (int)pi.dwProcessId);
        } else {
            Trace("Could not start shell");
        }
    }

    CloseHandle(conout1);
    CloseHandle(conout2);
    CloseHandle(conin);

    FreeConsole();

    /*
    // Now that the shell is started, tell the agent to shutdown when the
    // console has no more programs using it.
    AgentMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = AgentMsg::SetAutoShutDownFlag;
    msg.u.flag = TRUE;
    writeMsg(msg);
    */
    return ret;
#endif
    return 0;
}

// The lock should be acquired by the caller.
static void completeRead(pconsole_s *pc, DWORD amount)
{
    pc->dataReadAmount += amount;
    pc->ioCallbackFlag = true;
    SetEvent(pc->ioEvent);
}

// The lock should be acquired by the caller.
static void pumpReadIo(pconsole_s *pc)
{
    while (!pc->dataReadPending && pc->dataReadAmount < bufSize / 2) {
        if (pc->dataReadStart > 0) {
            if (pc->dataReadAmount > 0) {
                memmove(pc->dataReadBuffer,
                        pc->dataReadBuffer + pc->dataReadStart,
                        pc->dataReadAmount);
            }
            pc->dataReadStart = 0;
        }
        memset(&pc->dataReadOver, 0, sizeof(pc->dataReadOver));
        pc->dataReadOver.hEvent = pc->dataReadEvent;
        DWORD amount;
        BOOL ret = ReadFile(pc->dataPipe,
                            pc->dataReadBuffer + pc->dataReadAmount,
                            bufSize - pc->dataReadAmount,
                            &amount,
                            &pc->dataReadOver);
        if (ret) {
            completeRead(pc, amount);
        } else if (GetLastError() == ERROR_IO_PENDING) {
            pc->dataReadPending = true;
        } else {
            // TODO: The pipe is broken.
        }
    }
}

// The lock should be acquired by the caller.
static void completeWrite(pconsole_s *pc, DWORD amount)
{
    if (amount < pc->dataWriteAmount) {
        memmove(pc->dataWriteBuffer,
                pc->dataWriteBuffer + amount,
                pc->dataWriteAmount - amount);
    }
    pc->dataWriteAmount -= amount;
    pc->ioCallbackFlag = true;
    SetEvent(pc->ioEvent);
}

// The lock should be acquired by the caller.
static void pumpWriteIo(pconsole_s *pc)
{
    while (!pc->dataWritePending && pc->dataWriteAmount > 0) {
        memset(&pc->dataWriteOver, 0, sizeof(pc->dataWriteOver));
        pc->dataWriteOver.hEvent = pc->dataWriteEvent;
        DWORD amount;
        BOOL ret = WriteFile(pc->dataPipe,
                             pc->dataWriteBuffer,
                             pc->dataWriteAmount,
                             &amount,
                             &pc->dataWriteOver);
        if (ret) {
            completeWrite(pc, amount);
        } else if (GetLastError() == ERROR_IO_PENDING) {
            pc->dataWritePending = true;
        } else {
            // TODO: The pipe is broken.
        }
    }
}

static WINAPI DWORD serviceThread(void *threadParam)
{
    pconsole_s *pc = (pconsole_s*)threadParam;
    HANDLE events[] = { pc->dataReadEvent, pc->dataWriteEvent, pc->ioEvent };
    while (true) {
        EnterCriticalSection(&pc->lock);

        if (pc->dataReadPending) {
            DWORD amount;
            BOOL ret = GetOverlappedResult(pc->dataPipe, &pc->dataReadOver,
                                           &amount, FALSE);
            if (!ret && GetLastError() == ERROR_IO_INCOMPLETE) {
                // Keep waiting.
            } else if (!ret) {
                // TODO: Something is wrong.
            } else {
                completeRead(pc, amount);
                pc->dataReadPending = false;
                ResetEvent(pc->dataReadEvent);
            }
        }

        if (pc->dataWritePending) {
            DWORD amount;
            BOOL ret = GetOverlappedResult(pc->dataPipe, &pc->dataWriteOver,
                                           &amount, FALSE);
            if (!ret && GetLastError() == ERROR_IO_INCOMPLETE) {
                // Keep waiting.
            } else if (!ret) {
                // TODO: Something is wrong.
            } else {
                completeWrite(pc, amount);
                pc->dataWritePending = false;
                ResetEvent(pc->dataWriteEvent);
            }
        }

        pumpReadIo(pc);
        pumpWriteIo(pc);

        ResetEvent(pc->ioEvent);
        pconsole_io_cb iocb = pc->ioCallbackFlag ? pc->ioCallback : NULL;
        pc->ioCallbackFlag = false;

        LeaveCriticalSection(&pc->lock);

        if (iocb) {
            // Should this callback happen with the lock acquired?  With the
            // lock unacquired, then a library user could change the callback
            // function and still see a call to the old callback function.
            // With the lock acquired, a deadlock could happen if the user
            // acquires a lock in the callback.  In practice, I expect that
            // the callback routine will be NULL until it is initialized, and
            // it will only be initialized once.
            iocb(pc);
        }

        WaitForMultipleObjects(sizeof(events) / sizeof(events[0]), events,
                               FALSE, INFINITE);
    }
    return 0;
}

PCONSOLE_API int pconsole_read(pconsole_s *pconsole,
			       void *buffer,
			       int size)
{
    int ret;
    EnterCriticalSection(&pconsole->lock);
    int amount = std::min(size, pconsole->dataReadAmount);
    if (amount == 0) {
        ret = -1;
    } else {
        memcpy(buffer, pconsole->dataReadBuffer + pconsole->dataReadStart, amount);
        pconsole->dataReadStart += amount;
        pconsole->dataReadAmount -= amount;
        ret = amount;
        pumpReadIo(pconsole);
    }
    LeaveCriticalSection(&pconsole->lock);
    return ret;
}

PCONSOLE_API int pconsole_write(pconsole_s *pconsole,
				const void *buffer,
				int size)
{
    int ret;
    EnterCriticalSection(&pconsole->lock);
    int amount = std::min(size, bufSize - pconsole->dataWriteAmount);
    if (amount == 0) {
        ret = -1;
    } else {
        memcpy(pconsole->dataWriteBuffer + pconsole->dataWriteAmount,
               buffer,
               amount);
        pconsole->dataWriteAmount += amount;
        ret = amount;
        pumpWriteIo(pconsole);
    }
    LeaveCriticalSection(&pconsole->lock);
    return ret;
}

PCONSOLE_API int pconsole_set_size(pconsole_s *pconsole, int cols, int rows)
{
    // TODO: implement
    return 0;
}

/*
PCONSOLE_API int pconsole_get_output_queue_size(pconsole_s *pconsole)
{
    // TODO: replace this API...
    return 0;
}
*/

PCONSOLE_API void pconsole_close(pconsole_s *pconsole)
{
    CloseHandle(pconsole->dataPipe);
    //CloseHandle(pconsole->cancelEvent);
    CloseHandle(pconsole->dataReadEvent);
    CloseHandle(pconsole->dataWriteEvent);
    delete pconsole;
}
