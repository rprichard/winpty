#include "TestUtil.h"

#include <array>
#include <string>

#include "NtHandleQuery.h"
#include "RemoteHandle.h"
#include "RemoteWorker.h"
#include "UnicodeConversions.h"

#include <DebugClient.h>
#include <OsModule.h>
#include <WinptyAssert.h>

std::tuple<RemoteHandle, RemoteHandle> newPipe(
        RemoteWorker &w, BOOL inheritable) {
    HANDLE readPipe, writePipe;
    auto ret = CreatePipe(&readPipe, &writePipe, NULL, 0);
    ASSERT(ret && "CreatePipe failed");
    auto p1 = RemoteHandle::dup(readPipe, w, inheritable);
    auto p2 = RemoteHandle::dup(writePipe, w, inheritable);
    trace("Opened pipe in pid %u: rh=0x%I64x wh=0x%I64x",
        w.pid(), p1.uvalue(), p2.uvalue());
    CloseHandle(readPipe);
    CloseHandle(writePipe);
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

// Get the ObjectPointer (underlying NT object) for the NT handle.
void *ntHandlePointer(RemoteHandle h) {
    HANDLE ret = nullptr;
    for (auto &entry : queryNtHandles()) {
        if (entry.OwnerPid == h.worker().pid() &&
                entry.HandleValue == h.uvalue()) {
            ASSERT(ret == nullptr);
            ret = entry.ObjectPointer;
        }
    }
    return ret;
}

bool compareObjectHandles(RemoteHandle h1, RemoteHandle h2) {
    static auto kernelbaseCheck = LoadLibraryW(L"KernelBase.dll");
    if (kernelbaseCheck != nullptr) {
        static OsModule kernelbase(L"KernelBase.dll");
        static auto comp =
            reinterpret_cast<BOOL(WINAPI*)(HANDLE,HANDLE)>(
                kernelbase.proc("CompareObjectHandles"));
        if (comp != nullptr) {
            HANDLE h1local = nullptr;
            HANDLE h2local = nullptr;
            bool dup1 = DuplicateHandle(
                h1.worker().processHandle(),
                h1.value(),
                GetCurrentProcess(),
                &h1local,
                0, false, DUPLICATE_SAME_ACCESS);
            bool dup2 = DuplicateHandle(
                h2.worker().processHandle(),
                h2.value(),
                GetCurrentProcess(),
                &h2local,
                0, false, DUPLICATE_SAME_ACCESS);
            bool ret = dup1 && dup2 && comp(h1local, h2local);
            if (dup1) {
                CloseHandle(h1local);
            }
            if (dup2) {
                CloseHandle(h2local);
            }
            return ret;
        }
    }
    return ntHandlePointer(h1) == ntHandlePointer(h2);
}
