#pragma once

#include "FixedSizeString.h"
#include "Spawn.h"

struct Command {
    enum Kind {
        AllocConsole,
        AttachConsole,
        Close,
        DumpHandles,
        DumpScreenBuffers,
        Duplicate,
        Exit,
        FreeConsole,
        GetConsoleScreenBufferInfo,
        GetConsoleSelectionInfo,
        GetConsoleWindow,
        GetStdin,
        GetStderr,
        GetStdout,
        NewBuffer,
        OpenConOut,
        SetStdin,
        SetStderr,
        SetStdout,
        SetActiveBuffer,
        SpawnChild,
        System,
        WriteText,
    };

    Kind kind;
    HANDLE handle;
    HANDLE targetProcess;
    FixedSizeString<128> spawnName;
    SpawnParams spawnParams;
    FixedSizeString<1024> writeText;
    FixedSizeString<1024> systemText;
    DWORD dword;
    BOOL success;
    BOOL bInheritHandle;
    BOOL writeToEach;
    CONSOLE_SCREEN_BUFFER_INFO consoleScreenBufferInfo;
    CONSOLE_SELECTION_INFO consoleSelectionInfo;
    HWND hwnd;
};
