#include "TestUtil.h"

#include <array>
#include <string>

#include "RemoteHandle.h"
#include "RemoteWorker.h"
#include "UnicodeConversions.h"

#include <DebugClient.h>
#include <WinptyAssert.h>

std::tuple<RemoteHandle, RemoteHandle> newPipe(
        RemoteWorker &w, BOOL inheritable) {
    HANDLE readPipe, writePipe;
    auto ret = CreatePipe(&readPipe, &writePipe, NULL, 0);
    ASSERT(ret && "CreatePipe failed");
    auto p1 = RemoteHandle::dup(readPipe, w, inheritable);
    auto p2 = RemoteHandle::dup(writePipe, w, inheritable);
    return std::make_tuple(p1, p2);
}

void printTestName(const char *testName) {
    trace("----------------------------------------------------------");
    trace("%s", testName);
    printf("%s\n", testName);
    fflush(stdout);
}

std::string windowText(HWND hwnd) {
    std::array<wchar_t, 256> buf;
    DWORD ret = GetWindowTextW(hwnd, buf.data(), buf.size());
    ASSERT(ret >= 0 && ret <= buf.size() - 1);
    buf[ret] = L'\0';
    return narrowString(std::wstring(buf.data()));
}
