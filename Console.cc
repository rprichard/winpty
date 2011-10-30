#include "Console.h"
#include "AgentMessages.h"
#include "DebugClient.h"
#include <assert.h>

Console::Console() : m_pipeToAgent(NULL), m_pipeFromAgent(NULL)
{
}

void Console::start()
{
    BOOL success;

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE pipeToAgent;
    HANDLE pipeFromAgent;
    HANDLE pipeToClient;
    HANDLE pipeFromClient;
    success = CreatePipe(&pipeFromClient, &pipeToAgent, &sa, 0);
    assert(success);
    success = CreatePipe(&pipeFromAgent, &pipeToClient, &sa, 0);
    assert(success);

    m_pipeFromAgent = pipeFromAgent;
    m_pipeToAgent = pipeToAgent;

    STARTUPINFO sui;
    memset(&sui, 0, sizeof(sui));
    sui.cb = sizeof(sui);
    sui.dwFlags = STARTF_USESTDHANDLES;
    sui.hStdInput = pipeFromClient;
    sui.hStdOutput = pipeToClient;
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    // TODO: Don't use the CWD to find the agent.
    const char program[80] = ".\\ConsoleAgent.exe";
    char cmdline[80];
    strcpy(cmdline, program);
    success = CreateProcess(
        program,
        cmdline,
        NULL, NULL,
        /*bInheritHandles=*/TRUE,
        /*dwCreationFlags=*/CREATE_NO_WINDOW,
        NULL, NULL,
        &sui, &pi);
    if (!success) {
        Trace("Could not start agent subprocess.");
        exit(1);
    }
    Trace("New child process: PID %d", (int)pi.dwProcessId);

    CloseHandle(pipeFromClient);
    CloseHandle(pipeToClient);
}

AgentMessage *Console::request(AgentMessage *msg)
{
    DWORD actual = 0;
    BOOL success;
    success = WriteFile(m_pipeToAgent, msg, msg->size, &actual, NULL);
    assert(success);
    int replySize = 0;
    success = ReadFile(m_pipeFromAgent, &replySize, sizeof(replySize), &actual, NULL);
    assert(success);
    assert(replySize > 0);
    AgentMessage *reply = new char[replySize];
    assert(reply != NULL);
    reply->size = replySize;
    success = ReadFile(m_pipeFromAgent, &reply->data[0], replySize - sizeof(replySize), &actual, NULL);
    assert(success);
    return reply;
}
