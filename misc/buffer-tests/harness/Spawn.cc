#include "Spawn.h"

#include <string.h>

#include "UnicodeConversions.h"
#include "Util.h"

#include <DebugClient.h>
#include <OsModule.h>
#include <WinptyAssert.h>

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

    STARTUPINFOEXW suix = { params.sui };
    ASSERT(suix.StartupInfo.cb == sizeof(STARTUPINFOW) ||
           suix.StartupInfo.cb == sizeof(STARTUPINFOEXW));
    std::unique_ptr<char[]> attrListBuffer;
    auto inheritList = params.inheritList;
    LPPROC_THREAD_ATTRIBUTE_LIST attrList = nullptr;

    OsModule kernel32(L"kernel32.dll");
#define DECL_API_FUNC(name) decltype(name) *p##name = nullptr;
    DECL_API_FUNC(InitializeProcThreadAttributeList);
    DECL_API_FUNC(UpdateProcThreadAttribute);
    DECL_API_FUNC(DeleteProcThreadAttributeList);
#undef DECL_API_FUNC

    if (params.inheritCount != SpawnParams::NoInheritList) {
        // Add PROC_THREAD_ATTRIBUTE_HANDLE_LIST to the STARTUPINFOEX.  Use
        // dynamic binding, because this code must run on XP, which does not
        // have this functionality.
        ASSERT(params.inheritCount < params.inheritList.size());
#define GET_API_FUNC(name) p##name = reinterpret_cast<decltype(name)*>(kernel32.proc(#name));
        GET_API_FUNC(InitializeProcThreadAttributeList);
        GET_API_FUNC(UpdateProcThreadAttribute);
        GET_API_FUNC(DeleteProcThreadAttributeList);
#undef GET_API_FUNC
        if (pInitializeProcThreadAttributeList == nullptr ||
                pUpdateProcThreadAttribute == nullptr ||
                pDeleteProcThreadAttributeList == nullptr) {
            trace("Error: skipping PROC_THREAD_ATTRIBUTE_HANDLE_LIST "
                  "due to missing APIs");
        } else {
            SIZE_T bufferSize = 0;
            auto success = pInitializeProcThreadAttributeList(
                NULL, 1, 0, &bufferSize);
            if (!success && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                // The InitializeProcThreadAttributeList API "fails" with
                // ERROR_INSUFFICIENT_BUFFER.
                success = TRUE;
            }
            ASSERT(success &&
                "First InitializeProcThreadAttributeList call failed");
            attrListBuffer = std::unique_ptr<char[]>(new char[bufferSize]);
            suix.lpAttributeList = attrList =
                reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
                    attrListBuffer.get());
            success = pInitializeProcThreadAttributeList(
                suix.lpAttributeList, 1, 0, &bufferSize);
            ASSERT(success &&
                "Second InitializeProcThreadAttributeList call failed");
            success = pUpdateProcThreadAttribute(
                suix.lpAttributeList, 0,
                PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                inheritList.data(),
                params.inheritCount * sizeof(HANDLE),
                nullptr, nullptr);
            ASSERT(success && "UpdateProcThreadAttribute failed");
        }
    }

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    HANDLE ret = nullptr;
    auto success = CreateProcessW(workerPathWStr.c_str(), cmdLineWVec.data(),
                                  NULL, NULL,
                                  /*bInheritHandles=*/params.bInheritHandles,
                                  /*dwCreationFlags=*/params.dwCreationFlags,
                                  NULL, NULL,
                                  &suix.StartupInfo, &pi);
    if (!success) {
        trace("CreateProcessW failed: GetLastError=0x%x", GetLastError());
    } else {
        ret = pi.hProcess;
        CloseHandle(pi.hThread);
    }

    if (attrList != nullptr) {
        pDeleteProcThreadAttributeList(attrList);
    }

    return ret;
}
