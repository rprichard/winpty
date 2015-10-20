#pragma once

#include <windows.h>

#include <string>

struct SpawnParams {
    BOOL bInheritHandles = FALSE;
    DWORD dwCreationFlags = 0;
};

HANDLE spawn(const std::string &workerName, const SpawnParams &params);
