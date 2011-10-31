#ifndef CLIENT_H
#define CLIENT_H

#include <windows.h>
#include "AgentIo.h"

class Client
{
public:
    Client();
    void start();
    DWORD processId();
    HWND hwnd();

    void updateConsoleData();
    ConsoleData &consoleData();

    void writeConsoleInput(INPUT_RECORD *records, int count);

    void setTitle(const char *title);
    const char *getTitle();

private:
    void sendRequest(RequestKind kind);

private:
    HANDLE m_sharedMemoryMapping;
    AgentSharedMemory *m_sharedMemory;
    PROCESS_INFORMATION m_agentProcess;
};

#endif // CLIENT_H
