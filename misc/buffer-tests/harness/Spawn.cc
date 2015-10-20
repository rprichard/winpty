#include "Spawn.h"

#include <string.h>

#include "HookAssert.h"
#include "UnicodeConversions.h"
#include "Util.h"

#include <DebugClient.h>

namespace {

static std::vector<wchar_t> wstrToWVector(const std::wstring &str) {
    std::vector<wchar_t> ret;
    ret.resize(str.size() + 1);
    wmemcpy(ret.data(), str.c_str(), str.size() + 1);
    return ret;
}

} // anonymous namespace

HANDLE spawn(const std::string &workerName, const SpawnParams &params) {
    auto workerPath = pathDirName(getModuleFileName(NULL)) + "\\Worker.exe";
    const std::wstring workerPathWStr = widenString(workerPath);
    const std::string cmdLine = "\"" + workerPath + "\" " + workerName;
    auto cmdLineWVec = wstrToWVector(widenString(cmdLine));

    STARTUPINFOW sui;
    memset(&sui, 0, sizeof(sui));
    sui.cb = sizeof(sui);

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    BOOL ret = CreateProcessW(workerPathWStr.c_str(), cmdLineWVec.data(),
                   NULL, NULL,
                   /*bInheritHandles=*/params.bInheritHandles,
                   /*dwCreationFlags=*/params.dwCreationFlags,
                   NULL, NULL,
                   &sui, &pi);
    ASSERT(ret && "CreateProcessW failed");
    CloseHandle(pi.hThread);
    return pi.hProcess;
}
