#include <windows.h>
#include <assert.h>
#include "DebugClient.h"

// The broker should be started with a hidden console (CONSOLE_NO_WINDOW).
// It will communicate via asynchronous message passing on stdin and stdout.
int main()
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD requestSize;
    HANDLE requestEvent = CreateEvent(NULL, /*bManualReset=*/TRUE, /*bInitialState=*/FALSE);
    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));
    overlapped.hEvent = requestEvent;
    ReadFile(input, &requestSize, sizeof(requestSize), NULL, &overlapped);

    while (true) {
        Trace("waiting");
        DWORD ret = WaitForMultipleObjects(1, &input, FALSE, INFINITE);
        Trace("woke from waiting, ret == %d", (int)ret);
        assert(ret != WAIT_FAILED);

        DWORD actual;
        DWORD count = 0;
        BOOL success = ReadFile(input, &count, sizeof(count), &actual, NULL);
        assert(success);
        char *msg = new char[count];
        assert(msg != NULL);
        success = ReadFile(input, msg, count, &actual, NULL);
        assert(success);

        Trace("Receive msg of %d bytes", (int)count);

        // ...

        success = WriteFile(output, &count, sizeof(count), &actual, NULL);
        assert(success);
        success = WriteFile(output, msg, count, &actual, NULL);
        assert(success);

        Trace("Replied '%s'", msg);

    }
}
