#include "TestCommon.h"

#include <string>

#include "UnicodeConversions.h"

#include <DebugClient.h>
#include <WinptyAssert.h>

Handle Handle::dup(HANDLE h, Worker &target, BOOL bInheritHandle) {
    HANDLE targetHandle;
    BOOL success = DuplicateHandle(
        GetCurrentProcess(),
        h,
        target.m_process,
        &targetHandle,
        0, bInheritHandle, DUPLICATE_SAME_ACCESS);
    ASSERT(success && "DuplicateHandle failed");
    return Handle(targetHandle, target);
}

Handle &Handle::activate() {
    worker().cmd().handle = m_value;
    worker().rpc(Command::SetActiveBuffer);
    return *this;
}

void Handle::write(const std::string &msg) {
    worker().cmd().handle = m_value;
    worker().cmd().u.writeText = msg;
    worker().rpc(Command::WriteText);
}

void Handle::close() {
    worker().cmd().handle = m_value;
    worker().rpc(Command::Close);
}

Handle &Handle::setStdin() {
    worker().cmd().handle = m_value;
    worker().rpc(Command::SetStdin);
    return *this;
}

Handle &Handle::setStdout() {
    worker().cmd().handle = m_value;
    worker().rpc(Command::SetStdout);
    return *this;
}

Handle &Handle::setStderr() {
    worker().cmd().handle = m_value;
    worker().rpc(Command::SetStderr);
    return *this;
}

Handle Handle::dup(Worker &target, BOOL bInheritHandle) {
    HANDLE targetProcessFromSource;

    if (&target == &worker()) {
        targetProcessFromSource = GetCurrentProcess();
    } else {
        // Allow the source worker to see the target worker.
        targetProcessFromSource = INVALID_HANDLE_VALUE;
        BOOL success = DuplicateHandle(
            GetCurrentProcess(),
            target.m_process,
            worker().m_process,
            &targetProcessFromSource,
            0, FALSE, DUPLICATE_SAME_ACCESS);
        ASSERT(success && "Process handle duplication failed");
    }

    // Do the user-level duplication in the source process.
    worker().cmd().handle = m_value;
    worker().cmd().targetProcess = targetProcessFromSource;
    worker().cmd().bInheritHandle = bInheritHandle;
    worker().rpc(Command::Duplicate);
    HANDLE retHandle = worker().cmd().handle;

    if (&target != &worker()) {
        // Cleanup targetProcessFromSource.
        worker().cmd().handle = targetProcessFromSource;
        worker().rpc(Command::CloseQuietly);
        ASSERT(worker().cmd().success &&
            "Error closing remote process handle");
    }

    return Handle(retHandle, target);
}

CONSOLE_SCREEN_BUFFER_INFO Handle::screenBufferInfo() {
    CONSOLE_SCREEN_BUFFER_INFO ret;
    bool success = tryScreenBufferInfo(&ret);
    ASSERT(success && "GetConsoleScreenBufferInfo failed");
    return ret;
}

bool Handle::tryScreenBufferInfo(CONSOLE_SCREEN_BUFFER_INFO *info) {
    worker().cmd().handle = m_value;
    worker().rpc(Command::GetConsoleScreenBufferInfo);
    if (worker().cmd().success && info != nullptr) {
        *info = worker().cmd().u.consoleScreenBufferInfo;
    }
    return worker().cmd().success;
}

DWORD Handle::flags() {
    DWORD ret;
    bool success = tryFlags(&ret);
    ASSERT(success && "GetHandleInformation failed");
    return ret;
}

bool Handle::tryFlags(DWORD *flags) {
    worker().cmd().handle = m_value;
    worker().rpc(Command::GetHandleInformation);
    if (worker().cmd().success && flags != nullptr) {
        *flags = worker().cmd().dword;
    }
    return worker().cmd().success;
}

void Handle::setFlags(DWORD mask, DWORD flags) {
    bool success = trySetFlags(mask, flags);
    ASSERT(success && "SetHandleInformation failed");
}

bool Handle::trySetFlags(DWORD mask, DWORD flags) {
    worker().cmd().handle = m_value;
    worker().cmd().u.setFlags.mask = mask;
    worker().cmd().u.setFlags.flags = flags;
    worker().rpc(Command::SetHandleInformation);
    return worker().cmd().success;
}

wchar_t Handle::firstChar() {
    // The "first char" is useful for identifying which output buffer a handle
    // refers to.
    worker().cmd().handle = m_value;
    const SMALL_RECT region = {};
    auto &io = worker().cmd().u.consoleIo;
    io.bufferSize = { 1, 1 };
    io.bufferCoord = {};
    io.ioRegion = region;
    worker().rpc(Command::ReadConsoleOutput);
    ASSERT(worker().cmd().success);
    ASSERT(!memcmp(&io.ioRegion, &region, sizeof(region)));
    return io.buffer[0].Char.UnicodeChar;
}

Handle &Handle::setFirstChar(wchar_t ch) {
    // The "first char" is useful for identifying which output buffer a handle
    // refers to.
    worker().cmd().handle = m_value;
    const SMALL_RECT region = {};
    auto &io = worker().cmd().u.consoleIo;
    io.buffer[0].Char.UnicodeChar = ch;
    io.buffer[0].Attributes = 7;
    io.bufferSize = { 1, 1 };
    io.bufferCoord = {};
    io.ioRegion = region;
    worker().rpc(Command::WriteConsoleOutput);
    ASSERT(worker().cmd().success);
    ASSERT(!memcmp(&io.ioRegion, &region, sizeof(region)));
    return *this;
}

bool Handle::tryNumberOfConsoleInputEvents(DWORD *ret) {
    worker().cmd().handle = m_value;
    worker().rpc(Command::GetNumberOfConsoleInputEvents);
    if (worker().cmd().success && ret != nullptr) {
        *ret = worker().cmd().dword;
    }
    return worker().cmd().success;
}

static std::string timeString() {
    FILETIME fileTime;
    GetSystemTimeAsFileTime(&fileTime);
    auto ret = ((uint64_t)fileTime.dwHighDateTime << 32) |
                fileTime.dwLowDateTime;
    return std::to_string(ret);
}

static std::string newWorkerName() {
    static int workerCounter = 0;
    static auto initialTimeString = timeString();
    return std::string("WinptyBufferTests-") +
        std::to_string(static_cast<int>(GetCurrentProcessId())) + "-" +
        initialTimeString + "-" +
        std::to_string(++workerCounter);
}

Worker::Worker(const std::string &name) :
    m_name(name),
    m_parcel(name + "-shmem", ShmemParcel::CreateNew),
    m_startEvent(name + "-start"),
    m_finishEvent(name + "-finish")
{
    m_finishEvent.set();
}

Worker::Worker(SpawnParams params) : Worker(newWorkerName()) {
    m_process = spawn(m_name, params);
    // Perform an RPC just to ensure that the worker process is ready, and
    // the console exists, before returning.
    rpc(Command::GetStdin);
}

Worker Worker::child(SpawnParams params) {
    Worker ret(newWorkerName());
    cmd().u.spawn.spawnName = ret.m_name;
    cmd().u.spawn.spawnParams = params;
    rpc(Command::SpawnChild);
    BOOL dupSuccess = DuplicateHandle(
        m_process,
        cmd().handle,
        GetCurrentProcess(),
        &ret.m_process,
        0, FALSE, DUPLICATE_SAME_ACCESS);
    ASSERT(dupSuccess && "Worker::child: DuplicateHandle failed");
    rpc(Command::CloseQuietly);
    ASSERT(cmd().success && "Worker::child: CloseHandle failed");
    // Perform an RPC just to ensure that the worker process is ready, and
    // the console exists, before returning.
    ret.rpc(Command::GetStdin);
    return ret;
}

Worker::~Worker() {
    cleanup();
}

void Worker::cleanup() {
    if (m_valid) {
        cmd().dword = 0;
        rpcAsync(Command::Exit);
        DWORD result = WaitForSingleObject(m_process, INFINITE);
        ASSERT(result == WAIT_OBJECT_0 &&
            "WaitForSingleObject failed while killing worker");
        CloseHandle(m_process);
        m_valid = false;
    }
}

CONSOLE_SELECTION_INFO Worker::selectionInfo() {
    rpc(Command::GetConsoleSelectionInfo);
    ASSERT(cmd().success);
    return cmd().u.consoleSelectionInfo;
}

void Worker::dumpConsoleHandles(BOOL writeToEach) {
    cmd().writeToEach = writeToEach;
    rpc(Command::DumpConsoleHandles);
}

std::vector<Handle> Worker::scanForConsoleHandles() {
    rpc(Command::ScanForConsoleHandles);
    auto &rpcTable = cmd().u.scanForConsoleHandles;
    std::vector<Handle> ret;
    for (int i = 0; i < rpcTable.count; ++i) {
        ret.push_back(Handle(rpcTable.table[i], *this));
    }
    return ret;
}

bool Worker::setTitleInternal(const std::wstring &wstr) {
    ASSERT(wstr.size() < cmd().u.consoleTitle.size());
    ASSERT(wstr.size() == wcslen(wstr.c_str()));
    wcscpy(cmd().u.consoleTitle.data(), wstr.c_str());
    rpc(Command::SetConsoleTitle);
    return cmd().success;
}

std::string Worker::title() {
    std::array<wchar_t, 1024> buf;
    DWORD ret = titleInternal(buf, buf.size());
    ret = std::min<DWORD>(ret, buf.size() - 1);
    buf[std::min<size_t>(buf.size() - 1, ret)] = L'\0';
    return narrowString(std::wstring(buf.data()));
}

// This API is more low-level than typical, because GetConsoleTitleW is buggy
// in older versions of Windows, and this method is used to test the bugs.
DWORD Worker::titleInternal(std::array<wchar_t, 1024> &buf, DWORD bufSize) {
    cmd().dword = bufSize;
    cmd().u.consoleTitle = buf;
    rpc(Command::GetConsoleTitle);
    buf = cmd().u.consoleTitle;
    return cmd().dword;
}

std::vector<DWORD> Worker::consoleProcessList() {
    rpc(Command::GetConsoleProcessList);
    DWORD count = cmd().dword;
    ASSERT(count <= cmd().u.processList.size());
    return std::vector<DWORD>(
        &cmd().u.processList[0],
        &cmd().u.processList[count]);
}

void Worker::rpc(Command::Kind kind) {
    rpcImpl(kind);
    m_finishEvent.wait();
}

void Worker::rpcAsync(Command::Kind kind) {
    rpcImpl(kind);
}

void Worker::rpcImpl(Command::Kind kind) {
    m_finishEvent.wait();
    m_finishEvent.reset();
    cmd().kind = kind;
    m_startEvent.set();
}
