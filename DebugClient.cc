#include "DebugClient.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

static void SendToDebugServer(const char *message)
{
    char response[16];
    DWORD responseSize;
    CallNamedPipe(
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

void Trace(const char *format, ...)
{
    char message[1024];

    va_list ap;
    va_start(ap, format);
    vsnprintf(message, sizeof(message), format, ap);
    message[sizeof(message) - 1] = '\0';
    va_end(ap);

    SYSTEMTIME systemTime;
    GetSystemTime(&systemTime);
    int timeInSeconds = systemTime.wHour * 3600 + systemTime.wMinute * 60 + systemTime.wSecond;

    char moduleName[1024];
    moduleName[0] = '\0';
    GetModuleFileName(NULL, moduleName, sizeof(moduleName));
    const char *baseName = strrchr(moduleName, '\\');
    baseName = (baseName != NULL) ? baseName + 1 : moduleName;

    char fullMessage[1024];
    snprintf(fullMessage, sizeof(fullMessage),
             "[%05d.%03d %s,p%04d,t%04d]: %s",
             timeInSeconds, systemTime.wMilliseconds,
             baseName, (int)GetCurrentProcessId(), (int)GetCurrentThreadId(),
             message);

    SendToDebugServer(fullMessage);
}
