#pragma once

#include <windows.h>

#include <string>

struct SpawnParams {
    BOOL bInheritHandles = FALSE;
    DWORD dwCreationFlags = 0;
    STARTUPINFOW sui = { sizeof(STARTUPINFOW), 0 };
    static const size_t NoInheritList = static_cast<size_t>(~0);
    size_t inheritCount = NoInheritList;
    std::array<HANDLE, 1024> inheritList = {};

    SpawnParams(bool bInheritHandles=false, DWORD dwCreationFlags=0) :
        bInheritHandles(bInheritHandles),
        dwCreationFlags(dwCreationFlags)
    {
    }
};

HANDLE spawn(const std::string &workerName, const SpawnParams &params);
