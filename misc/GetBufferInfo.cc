#include <windows.h>

#include <stdio.h>

#include "TestUtil.cc"

int main() {
    const HANDLE conout = CreateFileW(L"CONOUT$",
                                      GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL, OPEN_EXISTING, 0, NULL);
    ASSERT(conout != INVALID_HANDLE_VALUE);

    CONSOLE_SCREEN_BUFFER_INFO info = {};
    BOOL ret = GetConsoleScreenBufferInfo(conout, &info);
    ASSERT(ret && "GetConsoleScreenBufferInfo failed");

    trace("srWindow={L=%d,T=%d,R=%d,B=%d}", info.srWindow.Left, info.srWindow.Top, info.srWindow.Right, info.srWindow.Bottom);
    printf("srWindow={L=%d,T=%d,R=%d,B=%d}\n", info.srWindow.Left, info.srWindow.Top, info.srWindow.Right, info.srWindow.Bottom);

    trace("dwSize=%d,%d", info.dwSize.X);
    printf("dwSize=%d,%d\n", info.dwSize.X);

    return 0;
}
