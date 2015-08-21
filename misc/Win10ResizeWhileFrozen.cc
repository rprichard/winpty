/*
 * Demonstrates a conhost hang that occurs when widening the console buffer
 * while selection is in progress.  The problem affects the new Windows 10
 * console, not the "legacy" console mode that Windows 10 also includes.
 *
 * First tested with:
 *  - Windows 10.0.10240
 *  - conhost.exe version 10.0.10240.16384
 *  - ConhostV1.dll version 10.0.10240.16384
 *  - ConhostV2.dll version 10.0.10240.16391
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "../shared/DebugClient.cc"

const int SC_CONSOLE_MARK = 0xFFF2;
const int SC_CONSOLE_SELECT_ALL = 0xFFF5;

static void setBufferSize(int x, int y) {
    COORD size = { x, y };
    HANDLE conout = GetStdHandle(STD_OUTPUT_HANDLE);
    BOOL success = SetConsoleScreenBufferSize(conout, size);
    trace("setBufferSize: (%d,%d), result=%d", x, y, success);
}

static void setWindowPos(int x, int y, int w, int h) {
    SMALL_RECT r = { x, y, x + w - 1, y + h - 1 };
    HANDLE conout = GetStdHandle(STD_OUTPUT_HANDLE);
    BOOL success = SetConsoleWindowInfo(conout, /*bAbsolute=*/TRUE, &r);
    trace("setWindowPos: (%d,%d,%d,%d), result=%d", x, y, w, h, success);
}

static void countDown(int sec) {
    for (int i = sec; i > 0; --i) {
        printf("%d.. ", i);
        fflush(stdout);
        Sleep(1000);
    }
    printf("\n");
}

// Launch this test program again, in a new console that we will destroy.
static void startChildProcess() {
    wchar_t program[1024];
    wchar_t cmdline[1024];
    GetModuleFileNameW(NULL, program, 1024);
    swprintf(cmdline, L"\"%s\" CHILD", program);

    STARTUPINFOW sui;
    PROCESS_INFORMATION pi;
    memset(&sui, 0, sizeof(sui));
    memset(&pi, 0, sizeof(pi));
    sui.cb = sizeof(sui);

    CreateProcessW(program, cmdline,
                   NULL, NULL,
                   /*bInheritHandles=*/FALSE,
                   /*dwCreationFlags=*/CREATE_NEW_CONSOLE,
                   NULL, NULL,
                   &sui, &pi);
}

static void performTest() {
    setWindowPos(0, 0, 1, 1);
    setBufferSize(80, 25);
    setWindowPos(0, 0, 80, 25);

    countDown(5);

    SendMessage(GetConsoleWindow(), WM_SYSCOMMAND, SC_CONSOLE_SELECT_ALL, 0);
    Sleep(2000);

    // This API call does not return.  In the console window, the "Select All"
    // operation appears to end.  The console window becomes non-responsive,
    // and the conhost.exe process must be killed from the Task Manager.
    // (Killing this test program or closing the console window is not
    // sufficient.)
    //
    // The same hang occurs whether line resizing is off or on.  It happens
    // with both "Mark" and "Select All".
    setBufferSize(120, 25);

    printf("Done...\n");
    Sleep(2000);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        startChildProcess();
    } else {
        performTest();
    }
}
