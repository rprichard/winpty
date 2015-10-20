#pragma once

#include "FixedSizeString.h"
#include "Spawn.h"

#include <array>

struct Command {
    enum Kind {
        AllocConsole,
        AttachConsole,
        Close,
        CloseQuietly,
        DumpConsoleHandles,
        DumpStandardHandles,
        Duplicate,
        Exit,
        FreeConsole,
        GetConsoleScreenBufferInfo,
        GetConsoleSelectionInfo,
        GetConsoleWindow,
        GetHandleInformation,
        GetNumberOfConsoleInputEvents,
        GetStdin,
        GetStderr,
        GetStdout,
        NewBuffer,
        OpenConin,
        OpenConout,
        ReadConsoleOutput,
        ScanForConsoleHandles,
        SetHandleInformation,
        SetStdin,
        SetStderr,
        SetStdout,
        SetActiveBuffer,
        SpawnChild,
        System,
        WriteConsoleOutput,
        WriteText,
    };

    Kind kind;
    HANDLE handle;
    HANDLE targetProcess;
    DWORD dword;
    BOOL success;
    BOOL bInheritHandle;
    BOOL writeToEach;
    HWND hwnd;
    union {
        CONSOLE_SCREEN_BUFFER_INFO consoleScreenBufferInfo;
        CONSOLE_SELECTION_INFO consoleSelectionInfo;
        struct {
            FixedSizeString<128> spawnName;
            SpawnParams spawnParams;
        } spawn;
        FixedSizeString<1024> writeText;
        FixedSizeString<1024> systemText;
        struct {
            DWORD mask;
            DWORD flags;
        } setFlags;
        struct {
            int count;
            std::array<HANDLE, 1024> table;
        } scanForConsoleHandles;
        struct {
            std::array<CHAR_INFO, 1024> buffer;
            COORD bufferSize;
            COORD bufferCoord;
            SMALL_RECT ioRegion;
        } consoleIo;
    } u;
};
