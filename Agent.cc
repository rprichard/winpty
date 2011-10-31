#include "AgentIo.h"
#include "DebugClient.h"
#include <windows.h>
#include <assert.h>

#define DIM(x) (sizeof(x) / sizeof((x)[0]))

static HANDLE coninHandle;
static HANDLE conoutHandle;

static void DoRequest(AgentSharedMemory *memory);


// The agent is started with a hidden console (CONSOLE_NO_WINDOW).
int main(int argc, char *argv[])
{
    assert(argc == 2);
    HANDLE mapping = OpenFileMapping(FILE_MAP_WRITE, FALSE, argv[1]);
    assert(mapping != NULL);
    AgentSharedMemory *sharedMemory = (AgentSharedMemory*)MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, 0);
    assert(sharedMemory != NULL);

    coninHandle = CreateFile(
                "CONIN$",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_EXISTING, 0, NULL);
    conoutHandle = CreateFile(
                "CONOUT$",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_EXISTING, 0, NULL);
    assert(coninHandle != NULL);
    assert(conoutHandle != NULL);

    HANDLE handles[2] = {
        sharedMemory->clientProcess,
        sharedMemory->requestEvent
    };

    sharedMemory->hwnd = GetConsoleWindow();

    while (true) {
        Trace("waiting");
        DWORD ret = WaitForMultipleObjects(DIM(handles), handles, FALSE, INFINITE);
        HANDLE signaled = NULL;
        if (ret >= WAIT_OBJECT_0 && (ret - WAIT_OBJECT_0) < DIM(handles))
            signaled = handles[ret - WAIT_OBJECT_0];
        if (signaled == sharedMemory->clientProcess) {
            // The client has exited.  The agent should exit.  What about the
            // console?  The client could have crashed, so I think we need to
            // kill the console itself somehow.
            // TODO: cleanup...
            break;
        } else if (signaled == sharedMemory->requestEvent) {
            DoRequest(sharedMemory);
            SetEvent(sharedMemory->replyEvent);
        } else {
            assert(false);
        }
    }

    Trace("agent shutting down");
    return 0;
}

static void DoRequest(AgentSharedMemory *memory)
{
    switch (memory->requestKind) {
        case rkInit:
            // Do nothing.
            break;
        case rkWriteConsoleInput: {
            DWORD actual = 0;
            memory->u.input.result = WriteConsoleInput(coninHandle,
                              memory->u.input.records,
                              memory->u.input.count,
                              &memory->u.input.resultWritten);
            break;
        }
        case rkGetConsoleTitle:
            GetConsoleTitle(memory->u.title, sizeof(memory->u.title));
            break;
        case rkSetConsoleTitle:
            SetConsoleTitle(memory->u.title);
            break;
        case rkRead: {
            CONSOLE_SCREEN_BUFFER_INFO &info = memory->read.info;
            GetConsoleScreenBufferInfo(conoutHandle, &info);
            COORD bufferSize, bufferCoord;
            SMALL_RECT readRegion;
            readRegion = info.srWindow;
            bufferSize.X = readRegion.Right - readRegion.Left + 1;
            bufferSize.Y = readRegion.Bottom - readRegion.Top + 1;
            bufferCoord.X = bufferCoord.Y = 0;
            ReadConsoleOutput(conoutHandle,
                              memory->read.windowContent,
                              bufferSize,
                              bufferCoord,
                              &readRegion);
            break;
        }
    }
}
