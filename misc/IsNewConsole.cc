#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "TestUtil.cc"

const int SC_CONSOLE_MARK = 0xFFF2;
const int SC_CONSOLE_SELECT_ALL = 0xFFF5;

static COORD getCursorPos(HANDLE conout) {
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    BOOL ret = GetConsoleScreenBufferInfo(conout, &info);
    ASSERT(ret && "GetConsoleScreenBufferInfo failed");
    return info.dwCursorPosition;
}

static void setCursorPos(HANDLE conout, COORD pos) {
    BOOL ret = SetConsoleCursorPosition(conout, pos);
    ASSERT(ret && "SetConsoleCursorPosition failed");
}

int main() {
    const HANDLE conout = openConout();
    const HWND hwnd = GetConsoleWindow();
    ASSERT(hwnd != NULL && "GetConsoleWindow() returned NULL");

    bool isWindows10NewConsole = false;
    COORD pos = getCursorPos(conout);
    setCursorPos(conout, { 1, 0 });

    {
        COORD posA = getCursorPos(conout);
        SendMessage(hwnd, WM_SYSCOMMAND, SC_CONSOLE_MARK, 0);
        COORD posB = getCursorPos(conout);
        isWindows10NewConsole = !memcmp(&posA, &posB, sizeof(posA));
        SendMessage(hwnd, WM_CHAR, 27, 0x00010001); // Send ESCAPE
    }

    setCursorPos(conout, pos);

    if (isWindows10NewConsole) {
        printf("New Windows 10 console\n");
    } else {
        printf("Legacy console\n");
    }

    return 0;
}
