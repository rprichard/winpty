#include "Client.h"
#include "DebugClient.h"
#include "AgentIo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define DIM(x) (sizeof(x) / sizeof((x)[0]))

Client::Client() :
    m_sharedMemoryMapping(NULL),
    m_sharedMemory(NULL)
{
}

void Client::start()
{
    BOOL success;

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    // TODO: note that a static counter makes this code less thread-safe.
    char mappingName[80];
    static int consoleCounter = 0;
    sprintf(mappingName, "AgentSharedMemory-%d-%d",
            (int)GetCurrentProcessId(),
            ++consoleCounter);
    m_sharedMemoryMapping = CreateFileMapping(
                INVALID_HANDLE_VALUE,
                &sa,
                PAGE_READWRITE,
                /*dwMaximumSizeHigh=*/0,
                /*dwMaximumSizeLow=*/sizeof(AgentSharedMemory),
                mappingName);
    assert(m_sharedMemoryMapping != NULL);
    m_sharedMemory = (AgentSharedMemory*)MapViewOfFile(m_sharedMemoryMapping, FILE_MAP_WRITE, 0, 0, 0);
    assert(m_sharedMemory != NULL);
    m_sharedMemory->Init();

    STARTUPINFO sui;
    memset(&sui, 0, sizeof(sui));
    sui.cb = sizeof(sui);
    sui.dwFlags = STARTF_USESHOWWINDOW;
    sui.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    // TODO: Don't use the CWD to find the agent.
    const char program[80] = ".\\Agent.exe";
    char cmdline[80];
    sprintf(cmdline, "%s %s", program, mappingName);
    success = CreateProcess(
        program,
        cmdline,
        NULL, NULL,
        /*bInheritHandles=*/TRUE,
        /*dwCreationFlags=*/CREATE_NEW_CONSOLE,
        NULL, NULL,
        &sui, &m_agentProcess);
    if (!success) {
        Trace("Could not start agent subprocess.");
        exit(1);
    }
    Trace("New child process: PID %d", (int)m_agentProcess.dwProcessId);

    // Make sure the agent has initialized itself before returning.
    sendRequest(rkInit);
}

DWORD Client::processId()
{
    return m_agentProcess.dwProcessId;
}

HWND Client::hwnd()
{
    return m_sharedMemory->hwnd;
}

void Client::updateConsoleData()
{
    sendRequest(rkRead);
}

ConsoleData &Client::consoleData()
{
    return m_sharedMemory->read;
}

void Client::writeConsoleInput(INPUT_RECORD *records, int count)
{
    assert(count <= DIM(m_sharedMemory->u.input.records));
    m_sharedMemory->u.input.count = count;
    memcpy(m_sharedMemory->u.input.records, records, sizeof(INPUT_RECORD) * count);
    sendRequest(rkWriteConsoleInput);
}

void Client::setTitle(const char *title)
{
    // TODO: buffer overflow, error handling
    strcpy(m_sharedMemory->u.title, title);
    sendRequest(rkSetConsoleTitle);
}

const char *Client::getTitle()
{
    // TODO: buffer overflow, error handling
    sendRequest(rkGetConsoleTitle);
    return m_sharedMemory->u.title;
}

void Client::sendRequest(RequestKind kind)
{
    m_sharedMemory->requestKind = kind;
    SetEvent(m_sharedMemory->requestEvent);
    WaitForSingleObject(m_sharedMemory->replyEvent, INFINITE);
}
