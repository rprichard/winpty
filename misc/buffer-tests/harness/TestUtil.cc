#include "TestUtil.h"

#include <array>
#include <iostream>
#include <string>
#include <vector>

#include "OsVersion.h"
#include "NtHandleQuery.h"
#include "RemoteHandle.h"
#include "RemoteWorker.h"
#include "UnicodeConversions.h"

#include <DebugClient.h>
#include <OsModule.h>
#include <WinptyAssert.h>

static RegistrationTable *g_testFunctions;

void printTestName(const std::string &name) {
    trace("----------------------------------------------------------");
    trace("%s", name.c_str());
    printf("%s\n", name.c_str());
    fflush(stdout);
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
        m_hasTable = true;
    }
}

void *ObjectSnap::object(RemoteHandle h) {
    if (!m_hasTable) {
        m_table = queryNtHandles();
    }
    return ntHandlePointer(m_table, h);
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

std::string windowText(HWND hwnd) {
    std::array<wchar_t, 256> buf;
    DWORD ret = GetWindowTextW(hwnd, buf.data(), buf.size());
    ASSERT(ret >= 0 && ret <= buf.size() - 1);
    buf[ret] = L'\0';
    return narrowString(std::wstring(buf.data()));
}

// Verify that the process' open console handle set is as expected from
// attaching to a new console.
//  * The set of console handles is exactly (0x3, 0x7, 0xb).
//  * The console handles are inheritable.
void checkInitConsoleHandleSet(RemoteWorker &proc) {
    CHECK(isTraditionalConio() && "checkInitConsoleHandleSet is not valid "
                                  "with modern conio");
    auto actualHandles = proc.scanForConsoleHandles();
    auto correctHandles = std::vector<uint64_t> { 0x3, 0x7, 0xb };
    if (handleInts(actualHandles) == correctHandles &&
            allInheritable(actualHandles)) {
        return;
    }
    proc.dumpConsoleHandles();
    CHECK(false && "checkInitConsoleHandleSet failed");
}

// Verify that the child's open console handle set is as expected from having
// just attached to or spawned from a source worker.
//  * The set of child handles should exactly match the set of inheritable
//    source handles.
//  * Every open child handle should be inheritable.
void checkInitConsoleHandleSet(RemoteWorker &child, RemoteWorker &source) {
    ASSERT(isTraditionalConio() && "checkInitConsoleHandleSet is not valid "
                                   "with modern conio");
    auto cvec = child.scanForConsoleHandles();
    auto cvecInherit = inheritableHandles(cvec);
    auto svecInherit = inheritableHandles(source.scanForConsoleHandles());
    auto hv = &handleValues;
    if (hv(cvecInherit) == hv(svecInherit) && allInheritable(cvec)) {
        return;
    }
    source.dumpConsoleHandles();
    child.dumpConsoleHandles();
    CHECK(false && "checkInitConsoleHandleSet failed");
}

// Returns true if the handle is a "usable console handle":
//  * The handle must be open.
//  * It must be a console handle.
//  * The process must have an attached console.
//  * With modern conio, the handle must be "unbound" or bound to the
//    currently attached console.
bool isUsableConsoleHandle(RemoteHandle h) {
    // XXX: It would be more efficient/elegant to use GetConsoleMode instead.
    return h.tryNumberOfConsoleInputEvents() || h.tryScreenBufferInfo();
}

bool isUsableConsoleInputHandle(RemoteHandle h) {
    return h.tryNumberOfConsoleInputEvents();
}

bool isUsableConsoleOutputHandle(RemoteHandle h) {
    return h.tryScreenBufferInfo();
}

bool isUnboundConsoleObject(RemoteHandle h) {
    // XXX: Consider what happens here with NULL, INVALID_HANDLE_OBJECT, junk,
    // etc.  I *think* it should work.
    ASSERT(isModernConio() && "isUnboundConsoleObject is not valid with "
                              "traditional conio");
    static RemoteWorker other{ SpawnParams {false, CREATE_NO_WINDOW} };
    auto dup = h.dup(other);
    bool ret = isUsableConsoleHandle(dup);
    dup.close();
    return ret;
}

// Verify that an optional subset of the STDIN/STDOUT/STDERR standard
// handles are new handles referring to new Unbound console objects.
void checkModernConsoleHandleInit(RemoteWorker &proc,
                                  bool in, bool out, bool err) {
    // List all the usable console handles that weren't just opened.
    std::vector<RemoteHandle> preExistingHandles;
    for (auto h : proc.scanForConsoleHandles()) {
        if ((in && h.value() == proc.getStdin().value()) ||
                (out && h.value() == proc.getStdout().value()) ||
                (err && h.value() == proc.getStderr().value())) {
            continue;
        }
        preExistingHandles.push_back(h);
    }
    ObjectSnap snap;
    auto checkNonReuse = [&](RemoteHandle h) {
        // The Unbound console objects that were just opened should not be
        // inherited from anywhere else -- they should be brand new objects.
        for (auto other : preExistingHandles) {
            CHECK(!snap.eq(h, other));
        }
    };

    if (in) {
        CHECK(isUsableConsoleInputHandle(proc.getStdin()));
        CHECK(isUnboundConsoleObject(proc.getStdin()));
        checkNonReuse(proc.getStdin());
    }
    if (out) {
        CHECK(isUsableConsoleOutputHandle(proc.getStdout()));
        CHECK(isUnboundConsoleObject(proc.getStdout()));
        checkNonReuse(proc.getStdout());
    }
    if (err) {
        CHECK(isUsableConsoleOutputHandle(proc.getStderr()));
        CHECK(isUnboundConsoleObject(proc.getStderr()));
        checkNonReuse(proc.getStderr());
    }
    if (out && err) {
        ObjectSnap snap;
        CHECK(proc.getStdout().value() != proc.getStderr().value());
        CHECK(snap.eq(proc.getStdout(), proc.getStderr()));
    }
}

// Wrapper around RemoteWorker::child that does the bare minimum to use an
// inherit list.  It creates an inheritable pipe, closes one end, and specifies
// the other end in an inherit list.  It closes the final pipe end in the
// parent and child before returning.
//
// This function is useful for testing the modern bInheritHandles=TRUE handle
// duplication functionality.
//
RemoteWorker childWithDummyInheritList(RemoteWorker &p, SpawnParams sp) {
    auto pipe = newPipe(p, true);
    std::get<0>(pipe).close();
    auto dummy = std::get<1>(pipe);
    sp.bInheritHandles = true;
    sp.dwCreationFlags |= EXTENDED_STARTUPINFO_PRESENT;
    sp.sui.cb = sizeof(STARTUPINFOEXW);
    sp.inheritCount = 1;
    sp.inheritList = { dummy.value() };
    auto c = p.child(sp);
    RemoteHandle::invent(dummy.value(), c).close();
    dummy.close();
    return c;
}
