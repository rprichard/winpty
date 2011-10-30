#include <windows.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "DebugClient.h"

#if 0
static void reopenHandles()
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
    HANDLE conout1 = CreateFile("CONOUT$",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &sa,
        OPEN_EXISTING,
        0, NULL);
    HANDLE conout2 = CreateFile("CONOUT$",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &sa,
        OPEN_EXISTING,
        0, NULL);
    HANDLE conin = CreateFile("CONIN$",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &sa,
        OPEN_EXISTING,
        0, NULL);
    assert(conin != NULL);
    assert(conout1 != NULL);
    assert(conout2 != NULL);
    BOOL success;
    success = SetStdHandle(STD_OUTPUT_HANDLE, conout1);
    assert(success);
    success = SetStdHandle(STD_ERROR_HANDLE, conout2);
    assert(success);
    success = SetStdHandle(STD_INPUT_HANDLE, conin);
    assert(success);
}
#endif

static void startInitialProcess(HANDLE *pipeFromAgentOut, HANDLE *pipeToAgentOut)
{
    BOOL success;

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE pipeToAgent;
    HANDLE pipeFromAgent;
    HANDLE pipeToClient;
    HANDLE pipeFromClient;
    success = CreatePipe(&pipeFromClient, &pipeToAgent, &sa, 0);
    assert(success);
    success = CreatePipe(&pipeFromAgent, &pipeToClient, &sa, 0);
    assert(success);

    *pipeFromAgentOut = pipeFromAgent;
    *pipeToAgentOut = pipeToAgent;

    STARTUPINFO sui;
    memset(&sui, 0, sizeof(sui));
    sui.cb = sizeof(sui);
    sui.dwFlags = STARTF_USESTDHANDLES;
    sui.hStdInput = pipeFromClient;
    sui.hStdOutput = pipeToClient;
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    // TODO: Don't use the CWD to find the agent.
    const char program[80] = ".\\ConsoleAgent.exe";
    char cmdline[80];
    strcpy(cmdline, program);
    success = CreateProcess(
        program,
        cmdline,
        NULL, NULL,
        /*bInheritHandles=*/TRUE,
        /*dwCreationFlags=*/CREATE_NO_WINDOW,
        NULL, NULL,
        &sui, &pi);
    if (!success) {
        Trace("Could not start cmd.exe subprocess.");
        exit(1);
    }
    Trace("New child process: PID %d", (int)pi.dwProcessId);

    CloseHandle(pipeFromClient);
    CloseHandle(pipeToClient);
}

int main()
{
    BOOL success;

    HANDLE pipeFromAgent, pipeToAgent;
    startInitialProcess(&pipeFromAgent, &pipeToAgent);

#if 0
    char msg[] = "hello!";
    DWORD count = strlen(msg) + 1;

    DWORD actual;

    success = WriteFile(pipeToAgent, &count, sizeof(count), &actual, NULL);
    assert(success);
    success = WriteFile(pipeToAgent, msg, count, &actual, NULL);
    assert(success);

    // ...

    success = ReadFile(pipeFromAgent, &count, sizeof(count), &actual, NULL);
    assert(success);
    char *reply = new char[count];
    assert(reply != NULL);
    success = ReadFile(pipeFromAgent, reply, count, &actual, NULL);
    assert(success);

    printf("Reply(%d): '%s'\n", (int)count, reply);

    delete [] reply;
#endif

    Sleep(3000);

    return 0;
}




#if 0
        CONSOLE_SCREEN_BUFFER_INFO info;
        GetConsoleScreenBufferInfo(console, &info);
        Trace("bufsize=[%d,%d] win@(%d,%d) winsize=[%d,%d] cur@(%d,%d)",
              info.dwSize.X, info.dwSize.Y,
              info.srWindow.Left, info.srWindow.Top,
              info.srWindow.Right - info.srWindow.Left + 1,
              info.srWindow.Bottom - info.srWindow.Top + 1,
              info.dwCursorPosition.X, info.dwCursorPosition.Y);
        COORD bufferSize = { info.dwSize.X, info.dwSize.Y };
        COORD dataStart = { 0, 0 };
        CHAR_INFO *data = new CHAR_INFO[info.dwSize.X * info.dwSize.Y];
        SMALL_RECT readRegion = { 0, 0, 79, 190 };
        BOOL ret = ReadConsoleOutput(console, data, bufferSize, dataStart, &readRegion);
        delete [] data;
        //Trace("ret=%d (%d,%d)", ret, readRegion.Left, readRegion.Top);
        Sleep(1000);
#endif
