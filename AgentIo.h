#ifndef AGENTIO_H
#define AGENTIO_H

#include <windows.h>

enum RequestKind {
    rkInit,
    rkWriteConsoleInput,
    rkGetConsoleTitle,
    rkSetConsoleTitle,
    rkRead,
};

struct ConsoleData {
    CONSOLE_SCREEN_BUFFER_INFO info;
    CHAR_INFO windowContent[65536 / sizeof(CHAR_INFO)];
};

struct AgentSharedMemory
{
    HANDLE clientProcess;
    HANDLE requestEvent;
    HANDLE replyEvent;

    HWND hwnd;

    RequestKind requestKind;
    ConsoleData read;

    union {
        struct {
            int count;
            INPUT_RECORD records[65536 / sizeof(INPUT_RECORD)];
            BOOL result;
            DWORD resultWritten;
        } input;
        char title[65536];
    } u;

    void Init();
};

#endif // AGENTIO_H
