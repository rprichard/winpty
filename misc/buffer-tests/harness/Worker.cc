#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#include <vector>

#include "Event.h"
#include "ShmemParcel.h"
#include "Spawn.h"
#include "WorkerApi.h"
#include <DebugClient.h>

static const char *g_prefix = "";

static const char *successOrFail(BOOL ret) {
    return ret ? "ok" : "FAILED";
}

static HANDLE openConout(BOOL bInheritHandle) {
    // If sa isn't provided, the handle defaults to not-inheritable.
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = bInheritHandle;

    trace("%sOpening CONOUT...", g_prefix);
    HANDLE conout = CreateFileW(L"CONOUT$",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                &sa,
                OPEN_EXISTING, 0, NULL);
    trace("%sOpening CONOUT... 0x%I64x", g_prefix, (int64_t)conout);
    return conout;
}

static HANDLE createBuffer(BOOL bInheritHandle) {
    // If sa isn't provided, the handle defaults to not-inheritable.
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = bInheritHandle;

    trace("%sCreating a new buffer...", g_prefix);
    HANDLE conout = CreateConsoleScreenBuffer(
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                &sa,
                CONSOLE_TEXTMODE_BUFFER, NULL);

    trace("%sCreating a new buffer... 0x%I64x", g_prefix, (int64_t)conout);
    return conout;
}

static void writeTest(HANDLE conout, const char *msg) {
    char writeData[256];
    sprintf(writeData, "%s%s\n", g_prefix, msg);

    trace("%sWriting to 0x%I64x: '%s'...",
        g_prefix, (int64_t)conout, msg);
    DWORD actual = 0;
    BOOL ret = WriteConsoleA(conout, writeData, strlen(writeData), &actual, NULL);
    trace("%sWriting to 0x%I64x: '%s'... %s",
        g_prefix, (int64_t)conout, msg,
        successOrFail(ret && actual == strlen(writeData)));
}

static void setConsoleActiveScreenBuffer(HANDLE conout) {
    trace("SetConsoleActiveScreenBuffer(0x%I64x) called...",
        (int64_t)conout);
    trace("SetConsoleActiveScreenBuffer(0x%I64x) called... %s",
        (int64_t)conout,
        successOrFail(SetConsoleActiveScreenBuffer(conout)));
}

static void dumpHandles() {
    trace("stdin=0x%I64x stdout=0x%I64x stderr=0x%I64x",
        (int64_t)GetStdHandle(STD_INPUT_HANDLE),
        (int64_t)GetStdHandle(STD_OUTPUT_HANDLE),
        (int64_t)GetStdHandle(STD_ERROR_HANDLE));
}

static std::vector<HANDLE> scanForScreenBuffers() {
    std::vector<HANDLE> ret;
    OSVERSIONINFO verinfo = {0};
    verinfo.dwOSVersionInfoSize = sizeof(verinfo);
    BOOL success = GetVersionEx(&verinfo);
    ASSERT(success && "GetVersionEx failed");
    uint64_t version =
        ((uint64_t)verinfo.dwMajorVersion << 32) | verinfo.dwMinorVersion;
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (version >= 0x600000002) {
        // As of Windows 8, console screen buffers are real kernel handles.
        for (unsigned int h = 0x4; h <= 0x1000; h += 4) {
            if (GetConsoleScreenBufferInfo((HANDLE)h, &info)) {
                ret.push_back((HANDLE)h);
            }
        }
    } else {
        for (unsigned int h = 0x1; h <= 0xfff; h += 1) {
            if (GetConsoleScreenBufferInfo((HANDLE)h, &info)) {
                ret.push_back((HANDLE)h);
            }
        }
    }
    return ret;
}

int main(int argc, char *argv[]) {
    std::string workerName = argv[1];

    ShmemParcelTyped<Command> parcel(workerName + "-shmem", ShmemParcel::OpenExisting);
    Event startEvent(workerName + "-start");
    Event finishEvent(workerName + "-finish");
    Command &cmd = parcel.value();

    dumpHandles();

    while (true) {
        startEvent.wait();
        startEvent.reset();
        switch (cmd.kind) {
            case Command::AllocConsole:
                trace("Calling AllocConsole...");
                cmd.success = AllocConsole();
                trace("Calling AllocConsole... %s",
                    successOrFail(cmd.success));
                break;
            case Command::AttachConsole:
                trace("Calling AttachConsole(%u)...",
                    (unsigned int)cmd.dword);
                cmd.success = AttachConsole(cmd.dword);
                trace("Calling AttachConsole(%u)... %s",
                    (unsigned int)cmd.dword, successOrFail(cmd.success));
                break;
            case Command::Close:
                trace("closing 0x%I64x...",
                    (int64_t)cmd.handle);
                cmd.success = CloseHandle(cmd.handle);
                trace("closing 0x%I64x... %s",
                    (int64_t)cmd.handle, successOrFail(cmd.success));
                break;
            case Command::DumpHandles:
                dumpHandles();
                break;
            case Command::DumpScreenBuffers: {
                std::string dumpLine = "";
                for (HANDLE h : scanForScreenBuffers()) {
                    char buf[32];
                    const char *inherit = "";
                    DWORD flags;
                    if (GetHandleInformation(h, &flags)) {
                        inherit = (flags & HANDLE_FLAG_INHERIT) ? "(I)" : "(N)";
                    }
                    sprintf(buf, "0x%I64x%s", (int64_t)h, inherit);
                    dumpLine += std::string(" ") + buf;
                    if (cmd.writeToEach) {
                        char msg[256];
                        sprintf(msg, "%d: Writing to 0x%I64x",
                            (int)GetCurrentProcessId(), (int64_t)h);
                        writeTest((HANDLE)h, msg);
                    }
                }
                trace("Valid screen buffers:%s", dumpLine.c_str());
                break;
            }
            case Command::Duplicate: {
                HANDLE sourceHandle = cmd.handle;
                cmd.success = DuplicateHandle(
                    GetCurrentProcess(),
                    sourceHandle,
                    cmd.targetProcess,
                    &cmd.handle,
                    0, cmd.bInheritHandle, DUPLICATE_SAME_ACCESS);
                if (!cmd.success) {
                    cmd.handle = INVALID_HANDLE_VALUE;
                }
                trace("dup 0x%I64x to pid %u... %s, 0x%I64x",
                    (int64_t)sourceHandle,
                    (unsigned int)GetProcessId(cmd.targetProcess),
                    successOrFail(cmd.success),
                    (int64_t)cmd.handle);
                break;
            }
            case Command::Exit:
                trace("exiting");
                ExitProcess(cmd.dword);
                break;
            case Command::FreeConsole:
                trace("Calling FreeConsole...");
                cmd.success = FreeConsole();
                trace("Calling FreeConsole... %s", successOrFail(cmd.success));
                break;
            case Command::GetConsoleScreenBufferInfo:
                memset(&cmd.consoleScreenBufferInfo, 0, sizeof(cmd.consoleScreenBufferInfo));
                cmd.success = GetConsoleScreenBufferInfo(
                    cmd.handle, &cmd.consoleScreenBufferInfo);
                break;
            case Command::GetConsoleSelectionInfo:
                memset(&cmd.consoleSelectionInfo, 0, sizeof(cmd.consoleSelectionInfo));
                cmd.success = GetConsoleSelectionInfo(&cmd.consoleSelectionInfo);
                break;
            case Command::GetConsoleWindow:
                cmd.hwnd = GetConsoleWindow();
                break;
            case Command::GetStdin:
                cmd.handle = GetStdHandle(STD_INPUT_HANDLE);
                break;
            case Command::GetStderr:
                cmd.handle = GetStdHandle(STD_ERROR_HANDLE);
                break;
            case Command::GetStdout:
                cmd.handle = GetStdHandle(STD_OUTPUT_HANDLE);
                break;
            case Command::NewBuffer:
                cmd.handle = createBuffer(cmd.bInheritHandle);
                break;
            case Command::OpenConOut:
                cmd.handle = openConout(cmd.bInheritHandle);
                break;
            case Command::SetStdin:
                SetStdHandle(STD_INPUT_HANDLE, cmd.handle);
                trace("setting stdin to 0x%I64x", (int64_t)cmd.handle);
                break;
            case Command::SetStderr:
                SetStdHandle(STD_ERROR_HANDLE, cmd.handle);
                trace("setting stderr to 0x%I64x", (int64_t)cmd.handle);
                break;
            case Command::SetStdout:
                SetStdHandle(STD_OUTPUT_HANDLE, cmd.handle);
                trace("setting stdout to 0x%I64x", (int64_t)cmd.handle);
                break;
            case Command::SetActiveBuffer:
                setConsoleActiveScreenBuffer(cmd.handle);
                break;
            case Command::SpawnChild:
                trace("Spawning child...");
                cmd.handle = spawn(cmd.spawnName.str(), cmd.spawnParams);
                trace("Spawning child... pid %u",
                    (unsigned int)GetProcessId(cmd.handle));
                break;
            case Command::System:
                cmd.dword = system(cmd.systemText.c_str());
                break;
            case Command::WriteText:
                writeTest(cmd.handle, cmd.writeText.c_str());
                break;
        }
        finishEvent.set();
    }
    return 0;
}
