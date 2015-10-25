#include "TestUtil.h"

#include <array>
#include <iostream>
#include <string>
#include <vector>

#include "NtHandleQuery.h"
#include "RemoteHandle.h"
#include "RemoteWorker.h"
#include "UnicodeConversions.h"

#include <DebugClient.h>
#include <OsModule.h>
#include <WinptyAssert.h>

static RegistrationTable *g_testFunctions;

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

void printTestName(const std::string &name) {
    trace("----------------------------------------------------------");
    trace("%s", name.c_str());
    printf("%s\n", name.c_str());
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
void *ntHandlePointer(const std::vector<SYSTEM_HANDLE_ENTRY> &table,
                      RemoteHandle h) {
    HANDLE ret = nullptr;
    for (auto &entry : table) {
        if (entry.OwnerPid == h.worker().pid() &&
                entry.HandleValue == h.uvalue()) {
            ASSERT(ret == nullptr);
            ret = entry.ObjectPointer;
        }
    }
    return ret;
}

bool hasBuiltinCompareObjectHandles() {
    static auto kernelbase = LoadLibraryW(L"KernelBase.dll");
    if (kernelbase != nullptr) {
        static auto proc = GetProcAddress(kernelbase, "CompareObjectHandles");
        if (proc != nullptr) {
            return true;
        }
    }
    return false;
}

bool compareObjectHandles(RemoteHandle h1, RemoteHandle h2) {
    if (hasBuiltinCompareObjectHandles()) {
        static OsModule kernelbase(L"KernelBase.dll");
        static auto comp =
            reinterpret_cast<BOOL(WINAPI*)(HANDLE,HANDLE)>(
                kernelbase.proc("CompareObjectHandles"));
        ASSERT(comp != nullptr);
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
    } else {
        auto table = queryNtHandles();
        return ntHandlePointer(table, h1) == ntHandlePointer(table, h2);
    }
}

ObjectSnap::ObjectSnap() {
    if (!hasBuiltinCompareObjectHandles()) {
        m_table = queryNtHandles();
    }
}

bool ObjectSnap::eq(std::initializer_list<RemoteHandle> handles) {
    if (handles.size() < 2) {
        return true;
    }
    if (hasBuiltinCompareObjectHandles()) {
        for (auto i = handles.begin() + 1; i < handles.end(); ++i) {
            if (!compareObjectHandles(*handles.begin(), *i)) {
                return false;
            }
        }
    } else {
        HANDLE first = ntHandlePointer(m_table, *handles.begin());
        for (auto i = handles.begin() + 1; i < handles.end(); ++i) {
            if (first != ntHandlePointer(m_table, *i)) {
                return false;
            }
        }
    }
    return true;
}

void registerTest(const std::string &name, bool (&cond)(), void (&func)()) {
    if (g_testFunctions == nullptr) {
        g_testFunctions = new RegistrationTable {};
    }
    for (auto &entry : *g_testFunctions) {
        // I think the compiler catches duplicates already, but just in case.
        ASSERT(&cond != std::get<1>(entry) || &func != std::get<2>(entry));
    }
    g_testFunctions->push_back(std::make_tuple(name, &cond, &func));
}

RegistrationTable registeredTests() {
    return *g_testFunctions;
}
