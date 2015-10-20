#pragma once

#include <windows.h>

#include <string>

#include "Event.h"
#include "ShmemParcel.h"
#include "Spawn.h"
#include "WorkerApi.h"
#include <DebugClient.h>

class Worker;

class Handle {
    friend class Worker;

private:
    Handle(HANDLE value, Worker &worker) : m_value(value), m_worker(&worker) {}

public:
    static Handle invent(HANDLE h, Worker &worker) { return Handle(h, worker); }
    static Handle invent(DWORD h, Worker &worker) { return Handle((HANDLE)h, worker); }
    void activate();
    void write(const std::string &msg);
    void close();
    Handle &setStdin();
    Handle &setStdout();
    Handle &setStderr();
    Handle dup(Worker &target, BOOL bInheritHandle=FALSE);
    Handle dup(BOOL bInheritHandle=FALSE) { return dup(worker(), bInheritHandle); }
    static Handle dup(HANDLE h, Worker &target, BOOL bInheritHandle=FALSE);
    CONSOLE_SCREEN_BUFFER_INFO screenBufferInfo();
    HANDLE value() { return m_value; }
    Worker &worker() { return *m_worker; }

private:
    HANDLE m_value;
    Worker *m_worker;
};

class Worker {
    friend class Handle;

private:
    Worker(const std::string &name);
public:
    Worker() : Worker(SpawnParams {}) {}
    Worker(SpawnParams params);
    Worker child() { return child(SpawnParams {}); }
    Worker child(const SpawnParams &params);
    ~Worker();

    // allow moving
    Worker(Worker &&other) :
        m_name(std::move(other.m_name)),
        m_parcel(std::move(other.m_parcel)),
        m_startEvent(std::move(other.m_startEvent)),
        m_finishEvent(std::move(other.m_finishEvent)),
        m_process(std::move(other.m_process))
    {
        other.m_moved = true;
    }
    Worker &operator=(Worker &&other) {
        m_name = std::move(other.m_name);
        m_parcel = std::move(other.m_parcel);
        m_startEvent = std::move(other.m_startEvent);
        m_finishEvent = std::move(other.m_finishEvent);
        m_process = std::move(other.m_process);
        other.m_moved = true;
        return *this;
    }

    // Commands
    Handle getStdin()                   { rpc(Command::GetStdin); return Handle(cmd().handle, *this); }
    Handle getStdout()                  { rpc(Command::GetStdout); return Handle(cmd().handle, *this); }
    Handle getStderr()                  { rpc(Command::GetStderr); return Handle(cmd().handle, *this); }
    void detach()                       { rpc(Command::FreeConsole); }
    void attach(Worker &worker)         { cmd().dword = GetProcessId(worker.m_process); rpc(Command::AttachConsole); }
    void alloc()                        { rpc(Command::AllocConsole); }
    void dumpHandles()                  { rpc(Command::DumpHandles); }
    int system(const std::string &arg)  { cmd().systemText = arg; rpc(Command::System); return cmd().dword; }
    HWND consoleWindow()                { rpc(Command::GetConsoleWindow); return cmd().hwnd; }

    CONSOLE_SELECTION_INFO selectionInfo();
    void dumpScreenBuffers(BOOL writeToEach=FALSE);

    Handle openConout(BOOL bInheritHandle=FALSE) {
        cmd().bInheritHandle = bInheritHandle;
        rpc(Command::OpenConOut);
        return Handle(cmd().handle, *this);
    }

    Handle newBuffer(BOOL bInheritHandle=FALSE) {
        cmd().bInheritHandle = bInheritHandle;
        rpc(Command::NewBuffer);
        return Handle(cmd().handle, *this);
    }

private:
    Command &cmd() { return m_parcel.value(); }
    void rpc(Command::Kind kind);
    void rpcAsync(Command::Kind kind);
    void rpcImpl(Command::Kind kind);

private:
    bool m_moved = false;
    std::string m_name;
    ShmemParcelTyped<Command> m_parcel;
    Event m_startEvent;
    Event m_finishEvent;
    HANDLE m_process = NULL;
};
