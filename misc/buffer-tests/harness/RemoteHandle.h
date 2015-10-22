#pragma once

#include <windows.h>

#include <cstdint>
#include <string>

class RemoteWorker;

class RemoteHandle {
    friend class RemoteWorker;

private:
    RemoteHandle(HANDLE value, RemoteWorker &worker) :
        m_value(value), m_worker(&worker)
    {
    }

public:
    static RemoteHandle invent(HANDLE h, RemoteWorker &worker) {
        return RemoteHandle(h, worker);
    }
    static RemoteHandle invent(uint64_t h, RemoteWorker &worker) {
        return RemoteHandle(reinterpret_cast<HANDLE>(h), worker);
    }
    RemoteHandle &activate();
    void write(const std::string &msg);
    void close();
    RemoteHandle &setStdin();
    RemoteHandle &setStdout();
    RemoteHandle &setStderr();
    RemoteHandle dup(RemoteWorker &target, BOOL bInheritHandle=FALSE);
    RemoteHandle dup(BOOL bInheritHandle=FALSE) {
        return dup(worker(), bInheritHandle);
    }
    static RemoteHandle dup(HANDLE h, RemoteWorker &target,
                            BOOL bInheritHandle=FALSE);
    CONSOLE_SCREEN_BUFFER_INFO screenBufferInfo();
    bool tryScreenBufferInfo(CONSOLE_SCREEN_BUFFER_INFO *info=nullptr);
    DWORD flags();
    bool tryFlags(DWORD *flags=nullptr);
    void setFlags(DWORD mask, DWORD flags);
    bool trySetFlags(DWORD mask, DWORD flags);
    wchar_t firstChar();
    RemoteHandle &setFirstChar(wchar_t ch);
    bool tryNumberOfConsoleInputEvents(DWORD *ret=nullptr);
    HANDLE value() const { return m_value; }
    uint64_t uvalue() const { return reinterpret_cast<uint64_t>(m_value); }
    RemoteWorker &worker() const { return *m_worker; }

private:
    HANDLE m_value;
    RemoteWorker *m_worker;
};
