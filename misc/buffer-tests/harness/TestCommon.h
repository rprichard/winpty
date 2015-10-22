#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "Event.h"
#include "ShmemParcel.h"
#include "Spawn.h"
#include "WorkerApi.h"
#include <DebugClient.h>
#include <UnicodeConversions.h>

class Worker;

class Handle {
    friend class Worker;

private:
    Handle(HANDLE value, Worker &worker) : m_value(value), m_worker(&worker) {}

public:
    static Handle invent(HANDLE h, Worker &worker) { return Handle(h, worker); }
    static Handle invent(uint64_t h, Worker &worker) { return Handle(reinterpret_cast<HANDLE>(h), worker); }
    Handle &activate();
    void write(const std::string &msg);
    void close();
    Handle &setStdin();
    Handle &setStdout();
    Handle &setStderr();
    Handle dup(Worker &target, BOOL bInheritHandle=FALSE);
    Handle dup(BOOL bInheritHandle=FALSE) { return dup(worker(), bInheritHandle); }
    static Handle dup(HANDLE h, Worker &target, BOOL bInheritHandle=FALSE);
    CONSOLE_SCREEN_BUFFER_INFO screenBufferInfo();
    bool tryScreenBufferInfo(CONSOLE_SCREEN_BUFFER_INFO *info=nullptr);
    DWORD flags();
    bool tryFlags(DWORD *flags=nullptr);
    void setFlags(DWORD mask, DWORD flags);
    bool trySetFlags(DWORD mask, DWORD flags);
    wchar_t firstChar();
    Handle &setFirstChar(wchar_t ch);
    bool tryNumberOfConsoleInputEvents(DWORD *ret=nullptr);
    HANDLE value() const { return m_value; }
    uint64_t uvalue() const { return reinterpret_cast<uint64_t>(m_value); }
    Worker &worker() const { return *m_worker; }

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
private:
    void cleanup();
public:

    // basic worker info
    HANDLE processHandle() { return m_process; }
    DWORD pid() { return GetProcessId(m_process); }

    // allow moving
    Worker(Worker &&other) :
        m_name(std::move(other.m_name)),
        m_parcel(std::move(other.m_parcel)),
        m_startEvent(std::move(other.m_startEvent)),
        m_finishEvent(std::move(other.m_finishEvent)),
        m_process(std::move(other.m_process))
    {
        other.m_valid = false;
    }
    Worker &operator=(Worker &&other) {
        cleanup();
        m_name = std::move(other.m_name);
        m_parcel = std::move(other.m_parcel);
        m_startEvent = std::move(other.m_startEvent);
        m_finishEvent = std::move(other.m_finishEvent);
        m_process = std::move(other.m_process);
        other.m_valid = false;
        m_valid = true;
        return *this;
    }

    // Commands
    Handle getStdin()                   { rpc(Command::GetStdin); return Handle(cmd().handle, *this); }
    Handle getStdout()                  { rpc(Command::GetStdout); return Handle(cmd().handle, *this); }
    Handle getStderr()                  { rpc(Command::GetStderr); return Handle(cmd().handle, *this); }
    bool detach()                       { rpc(Command::FreeConsole); return cmd().success; }
    bool attach(Worker &worker)         { cmd().dword = GetProcessId(worker.m_process); rpc(Command::AttachConsole); return cmd().success; }
    bool alloc()                        { rpc(Command::AllocConsole); return cmd().success; }
    void dumpStandardHandles()          { rpc(Command::DumpStandardHandles); }
    int system(const std::string &arg)  { cmd().u.systemText = arg; rpc(Command::System); return cmd().dword; }
    HWND consoleWindow()                { rpc(Command::GetConsoleWindow); return cmd().hwnd; }

    CONSOLE_SELECTION_INFO selectionInfo();
    void dumpConsoleHandles(BOOL writeToEach=FALSE);
    std::vector<Handle> scanForConsoleHandles();
    void setTitle(const std::string &str)       { auto b = setTitleInternal(widenString(str)); ASSERT(b && "setTitle failed"); }
    bool setTitleInternal(const std::wstring &str);
    DWORD getTitleInternal(std::array<wchar_t, 1024> &buf, DWORD bufSize);

    Handle openConin(BOOL bInheritHandle=FALSE) {
        cmd().bInheritHandle = bInheritHandle;
        rpc(Command::OpenConin);
        return Handle(cmd().handle, *this);
    }

    Handle openConout(BOOL bInheritHandle=FALSE) {
        cmd().bInheritHandle = bInheritHandle;
        rpc(Command::OpenConout);
        return Handle(cmd().handle, *this);
    }

    Handle newBuffer(BOOL bInheritHandle=FALSE, wchar_t firstChar=L'\0') {
        cmd().bInheritHandle = bInheritHandle;
        rpc(Command::NewBuffer);
        auto h = Handle(cmd().handle, *this);
        if (firstChar != L'\0') {
            h.setFirstChar(firstChar);
        }
        return h;
    }

private:
    Command &cmd() { return m_parcel.value(); }
    void rpc(Command::Kind kind);
    void rpcAsync(Command::Kind kind);
    void rpcImpl(Command::Kind kind);

private:
    bool m_valid = true;
    std::string m_name;
    ShmemParcelTyped<Command> m_parcel;
    Event m_startEvent;
    Event m_finishEvent;
    HANDLE m_process = NULL;
};
