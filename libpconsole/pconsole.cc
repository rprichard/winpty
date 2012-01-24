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

struct Console {
    Console();
    HANDLE pipe;
    int agentPid;
    HANDLE cancelEvent;
    HANDLE readEvent;
    HANDLE writeEvent;
};

Console::Console() : pipe(NULL), agentPid(-1)
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

PSEUDOCONSOLE_DLLEXPORT
Console *consoleOpen(int cols, int rows)
{
    BOOL success;

    Console *console = new Console;

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
    console->pipe = CreateNamedPipe(serverName.c_str(),
                    /*dwOpenMode=*/PIPE_ACCESS_DUPLEX |
                                        FILE_FLAG_FIRST_PIPE_INSTANCE |
                                        FILE_FLAG_OVERLAPPED,
                    /*dwPipeMode=*/0,
                    /*nMaxInstances=*/1,
                    /*nOutBufferSize=*/0,
                    /*nInBufferSize=*/0,
                    /*nDefaultTimeOut=*/3000,
                    NULL);
    if (console->pipe == INVALID_HANDLE_VALUE)
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
    console->agentPid = pi.dwProcessId;
    //qDebug("New child process: PID %d", (int)m_agentProcess->dwProcessId);

    // Connect the named pipe.
    OVERLAPPED over;
    memset(&over, 0, sizeof(over));
    over.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    assert(over.hEvent != NULL);
    success = ConnectNamedPipe(console->pipe, &over);
    if (!success && GetLastError() == ERROR_IO_PENDING) {
        DWORD actual;
        success = GetOverlappedResult(console->pipe, &over, &actual, TRUE);
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
    console->cancelEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    console->readEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    console->writeEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    return console;
}

PSEUDOCONSOLE_DLLEXPORT
int consoleStartShell(Console *console,
                      const wchar_t *program,
                      const wchar_t *cmdline,
                      const wchar_t *cwd)
{
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
}

// Once I/O is canceled, read/write attempts return immediately.
PSEUDOCONSOLE_DLLEXPORT
void consoleCancelIo(Console *console)
{
    SetEvent(console->cancelEvent);
}

static int consoleIo(Console *console, void *buffer, int size, bool isRead)
{
    HANDLE event = isRead ? console->readEvent : console->writeEvent;
    OVERLAPPED over;
    memset(&over, 0, sizeof(over));
    over.hEvent = event;
    DWORD actual;
    BOOL success;
    if (isRead)
        success = ReadFile(console->pipe, buffer, size, &actual, &over);
    else
        success = WriteFile(console->pipe, buffer, size, &actual, &over);
    if (!success && GetLastError() == ERROR_IO_PENDING) {
        HANDLE handles[2] = { event, console->cancelEvent };
        int waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE) -
                WAIT_OBJECT_0;
        if (waitResult == 0) {
            success = GetOverlappedResult(console->pipe, &over, &actual, TRUE);
        } else if (waitResult == 1) {
            CancelIo(console->pipe);
            GetOverlappedResult(console->pipe, &over, &actual, TRUE);
            success = FALSE;
        } else {
            assert(false);
            success = FALSE;
        }
    }
    return success ? (int)actual : -1;
}

PSEUDOCONSOLE_DLLEXPORT
int consoleRead(Console *console, void *buffer, int size)
{
    return consoleIo(console, buffer, size, true);
}

PSEUDOCONSOLE_DLLEXPORT
int consoleWrite(Console *console, const void *buffer, int size)
{
    return consoleIo(console, (void*)buffer, size, false);
}

PSEUDOCONSOLE_DLLEXPORT
void consoleFree(Console *console)
{
    CloseHandle(console->pipe);
    CloseHandle(console->cancelEvent);
    CloseHandle(console->readEvent);
    CloseHandle(console->writeEvent);
    delete console;
}
