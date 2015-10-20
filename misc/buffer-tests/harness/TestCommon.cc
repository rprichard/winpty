#include "TestCommon.h"

#include <assert.h>

#include <string>

#include <DebugClient.h>

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

void Handle::activate() {
    worker().cmd().handle = m_value;
    worker().rpc(Command::SetActiveBuffer);
}

void Handle::write(const std::string &msg) {
    worker().cmd().handle = m_value;
    worker().cmd().writeText = msg;
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
        BOOL ret = DuplicateHandle(
            GetCurrentProcess(),
            target.m_process,
            worker().m_process,
            &targetProcessFromSource,
            0, FALSE, DUPLICATE_SAME_ACCESS);
        ASSERT(ret && "Process handle duplication failed");
    }

    // Do the user-level duplication in the source process.
    worker().cmd().handle = m_value;
    worker().cmd().targetProcess = targetProcessFromSource;
    worker().cmd().bInheritHandle = bInheritHandle;
    worker().rpc(Command::Duplicate);

    if (&target != &worker()) {
        // Cleanup targetProcessFromSource.
        worker().cmd().handle = targetProcessFromSource;
        worker().rpc(Command::Close);
        ASSERT(worker().cmd().success);
    }

    return Handle(worker().cmd().handle, target);
}

CONSOLE_SCREEN_BUFFER_INFO Handle::screenBufferInfo() {
    worker().cmd().handle = m_value;
    worker().rpc(Command::GetConsoleScreenBufferInfo);
    ASSERT(worker().cmd().success);
    return worker().cmd().consoleScreenBufferInfo;
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
    params.dwCreationFlags |= CREATE_NEW_CONSOLE;
    m_process = spawn(m_name, params);
}

Worker Worker::child(const SpawnParams &params) {
    Worker ret(newWorkerName());
    cmd().spawnName = ret.m_name;
    cmd().spawnParams = params;
    rpc(Command::SpawnChild);
    BOOL dupSuccess = DuplicateHandle(
        m_process,
        cmd().handle,
        GetCurrentProcess(),
        &ret.m_process,
        0, FALSE, DUPLICATE_SAME_ACCESS);
    ASSERT(dupSuccess && "DuplicateHandle failed");
    rpc(Command::Close);
    return ret;
}

Worker::~Worker() {
    if (!m_moved) {
        cmd().dword = 0;
        rpcAsync(Command::Exit);
        DWORD result = WaitForSingleObject(m_process, INFINITE);
        ASSERT(result == WAIT_OBJECT_0 &&
            "WaitForSingleObject failed while killing worker");
        CloseHandle(m_process);
    }
}

CONSOLE_SELECTION_INFO Worker::selectionInfo() {
    rpc(Command::GetConsoleSelectionInfo);
    ASSERT(cmd().success);
    return cmd().consoleSelectionInfo;
}

void Worker::dumpScreenBuffers(BOOL writeToEach) {
    cmd().writeToEach = writeToEach;
    rpc(Command::DumpScreenBuffers);
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
