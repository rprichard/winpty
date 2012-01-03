#include "DebugClient.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

static void SendToDebugServer(const char *message)
{
    char response[16];
    DWORD responseSize;
    CallNamedPipeA(
        "\\\\.\\pipe\\DebugServer",
        (void*)message, strlen(message),
        response, sizeof(response), &responseSize,
        NMPWAIT_WAIT_FOREVER);
}

void TraceRaw(const char *format, ...)
{
    char message[1024];

    va_list ap;
    va_start(ap, format);
    vsnprintf(message, sizeof(message), format, ap);
    message[sizeof(message) - 1] = '\0';
    va_end(ap);

    SendToDebugServer(message);
}

// Get the current UTC time as milliseconds from the epoch (ignoring leap
// seconds).  Use the Unix epoch for consistency with DebugClient.py.  There
// are 134774 days between 1601-01-01 (the Win32 epoch) and 1970-01-01 (the
// Unix epoch).
static long long UnixTimeMillis()
{
    FILETIME fileTime;
    GetSystemTimeAsFileTime(&fileTime);
    long long msTime = (((long long)fileTime.dwHighDateTime << 32) +
                       fileTime.dwLowDateTime) / 10000;
    return msTime - 134774LL * 24 * 3600 * 1000;
}

void Trace(const char *format, ...)
{
    char message[1024];

    va_list ap;
    va_start(ap, format);
    vsnprintf(message, sizeof(message), format, ap);
    message[sizeof(message) - 1] = '\0';
    va_end(ap);

    const int currentTime = (int)(UnixTimeMillis() % (100000 * 1000));

    char moduleName[1024];
    moduleName[0] = '\0';
    GetModuleFileNameA(NULL, moduleName, sizeof(moduleName));
    const char *baseName = strrchr(moduleName, '\\');
    baseName = (baseName != NULL) ? baseName + 1 : moduleName;

    char fullMessage[1024];
    snprintf(fullMessage, sizeof(fullMessage),
             "[%05d.%03d %s,p%04d,t%04d]: %s",
             currentTime / 1000, currentTime % 1000,
             baseName, (int)GetCurrentProcessId(), (int)GetCurrentThreadId(),
             message);

    SendToDebugServer(fullMessage);
}
