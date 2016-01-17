// Copyright (c) 2011-2015 Ryan Prichard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#define COMPILING_WINPTY_DLL

#include "../include/winpty.h"

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "../shared/AgentMsg.h"
#include "../shared/Buffer.h"
#include "../shared/DebugClient.h"
#include "../shared/GenRandom.h"
#include "../shared/StringBuilder.h"
#include "../shared/WindowsSecurity.h"
#include "BackgroundDesktop.h"
#include "Util.h"
#include "WinptyException.h"
#include "WinptyInternal.h"

using namespace libwinpty;
using namespace winpty_shared;

// Work around a bug with mingw-gcc-g++.  mingw-w64 is unaffected.  See
// GitHub issue 27.
#ifndef FILE_FLAG_FIRST_PIPE_INSTANCE
#define FILE_FLAG_FIRST_PIPE_INSTANCE 0x00080000
#endif

#define AGENT_EXE L"winpty-agent.exe"



/*****************************************************************************
 * Error handling -- translate C++ exceptions to an optional error object
 * output and log the result. */

WINPTY_API winpty_result_t winpty_error_code(winpty_error_ptr_t err) {
    return err != nullptr ? err->code
                          : WINPTY_ERROR_INVALID_ARGUMENT;
}

WINPTY_API LPCWSTR winpty_error_msg(winpty_error_ptr_t err) {
    return err != nullptr ? err->msg
                          : L"winpty_error_str argument is NULL";
}

WINPTY_API void winpty_error_free(winpty_error_ptr_t err) {
    if (err != nullptr && !err->errorIsStatic) {
        freeWStr(err->msg);
        delete err;
    }
}

STATIC_ERROR(kInvalidArgument,
    WINPTY_ERROR_INVALID_ARGUMENT,
    L"invalid argument"
);

STATIC_ERROR(kBadRpcPacket,
    WINPTY_ERROR_INTERNAL_ERROR,
    L"bad RPC packet"
);

STATIC_ERROR(kUncaughtException,
    WINPTY_ERROR_INTERNAL_ERROR,
    L"uncaught C++ exception"
);

static void translateException(winpty_error_ptr_t *&err) {
    winpty_error_ptr_t ret = nullptr;
    try {
        throw;
    } catch (WinptyException &e) {
        ret = e.release();
    } catch (const ReadBuffer::DecodeError &e) {
        ret = const_cast<winpty_error_ptr_t>(&kBadRpcPacket);
    } catch (const std::bad_alloc &e) {
        ret = const_cast<winpty_error_ptr_t>(&kOutOfMemory);
    } catch (...) {
        ret = const_cast<winpty_error_ptr_t>(&kUncaughtException);
    }
    trace("libwinpty error: code=%u msg='%ls'",
        static_cast<unsigned>(ret->code), ret->msg);
    if (err != nullptr) {
        *err = ret;
    } else {
        winpty_error_free(ret);
    }
}

static void throwInvalidArgument() {
    throwStaticError(kInvalidArgument);
}

static inline void require_arg(bool cond) {
    if (!cond) {
        throwInvalidArgument();
    }
}

#define API_TRY \
    if (err != nullptr) { *err = nullptr; } \
    try

#define API_CATCH(ret) \
    catch (...) { translateException(err); return (ret); }



/*****************************************************************************
 * Configuration of a new agent. */

WINPTY_API winpty_config_t *
winpty_config_new(DWORD flags, winpty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        require_arg((flags & WINPTY_FLAG_MASK) == flags);
        std::unique_ptr<winpty_config_t> ret(new winpty_config_t);
        ret->flags = flags;
        return ret.release();
    } API_CATCH(nullptr)
}

WINPTY_API void winpty_config_free(winpty_config_t *cfg) {
    delete cfg;
}

WINPTY_API BOOL
winpty_config_set_initial_size(winpty_config_t *cfg, int cols, int rows,
                               winpty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        require_arg(cfg != nullptr && cols > 0 && rows > 0);
        cfg->cols = cols;
        cfg->rows = rows;
        return TRUE;
    } API_CATCH(FALSE);
}

WINPTY_API BOOL
winpty_config_set_agent_timeout(winpty_config_t *cfg, DWORD timeoutMs,
                                winpty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        require_arg(cfg != nullptr && timeoutMs > 0);
        cfg->timeoutMs = timeoutMs;
        return TRUE;
    } API_CATCH(FALSE)
}

/*****************************************************************************
 * Start the agent. */

static std::wstring findAgentProgram() {
    std::wstring progDir = dirname(getModuleFileName(getCurrentModule()));
    std::wstring ret = progDir + (L"\\" AGENT_EXE);
    if (!pathExists(ret)) {
        throwWinptyException(
            WINPTY_ERROR_AGENT_EXE_MISSING,
            L"agent executable does not exist: '" + ret + L"'");
    }
    return ret;
}

static void handlePendingIo(winpty_t *wp, OVERLAPPED &over, BOOL &success,
                            DWORD &actual) {
    if (!success && GetLastError() == ERROR_IO_PENDING) {
        PendingIo io(wp->controlPipe.get(), over);
        const HANDLE waitHandles[2] = { wp->ioEvent.get(),
                                        wp->agentProcess.get() };
        DWORD waitRet = WaitForMultipleObjects(
            2, waitHandles, FALSE, wp->agentTimeoutMs);
        // TODO: interesting edge case to test; what if the client
        // disconnects after we wake up and before we call
        // GetOverlappedResult?  I predict either:
        //  - the connect succeeds
        //  - the connect fails with ERROR_BROKEN_PIPE
        if (waitRet != WAIT_OBJECT_0) {
            // The I/O is still pending.  Cancel it, close the I/O event, and
            // throw an exception.
            if (waitRet == WAIT_OBJECT_0 + 1) {
                throwWinptyException(WINPTY_ERROR_AGENT_DIED, L"agent died");
            } else if (waitRet == WAIT_TIMEOUT) {
                throwWinptyException(WINPTY_ERROR_AGENT_TIMEOUT,
                                      L"agent timed out");
            } else if (waitRet == WAIT_FAILED) {
                throwLastWindowsError(L"WaitForMultipleObjects failed");
            } else {
                throwWinptyException(WINPTY_ERROR_INTERNAL_ERROR,
                    L"unexpected WaitForMultipleObjects return value");
            }
        }
        success = io.waitForCompletion(actual);
    }
}

static void handlePendingIo(winpty_t *wp, OVERLAPPED &over, BOOL &success) {
    DWORD actual = 0;
    handlePendingIo(wp, over, success, actual);
}

static void handleReadWriteErrors(winpty_t *wp, BOOL success,
                                  const wchar_t *genericErrMsg) {
    if (!success) {
        // TODO: We failed during the write.  We *probably* should permanently
        // shut things down, disconnect at least the control pipe.
        // TODO: Which errors, *specifically*, do we care about?
        const DWORD lastError = GetLastError();
        if (lastError == ERROR_BROKEN_PIPE || lastError == ERROR_NO_DATA ||
                lastError == ERROR_PIPE_NOT_CONNECTED) {
            throwWinptyException(WINPTY_ERROR_LOST_CONNECTION,
                L"lost connection to agent");
        } else {
            throwLastWindowsError(genericErrMsg);
        }
    }
}

// Calls ConnectNamedPipe to wait until the agent connects to the control pipe.
static void
connectControlPipe(winpty_t *wp) {
    OVERLAPPED over = {};
    over.hEvent = wp->ioEvent.get();
    BOOL success = ConnectNamedPipe(wp->controlPipe.get(), &over);
    handlePendingIo(wp, over, success);
    if (!success && GetLastError() == ERROR_PIPE_CONNECTED) {
        success = TRUE;
    }
    if (!success) {
        throwLastWindowsError(L"ConnectNamedPipe failed");
    }
}

static void writeData(winpty_t *wp, const void *data, size_t amount) {
    // Perform a single pipe write.
    DWORD actual = 0;
    OVERLAPPED over = {};
    over.hEvent = wp->ioEvent.get();
    BOOL success = WriteFile(wp->controlPipe.get(), data, amount,
                             &actual, &over);
    if (!success) {
        handlePendingIo(wp, over, success, actual);
        handleReadWriteErrors(wp, success, L"WriteFile failed");
    }
    if (actual != amount) {
        // TODO: We failed during the write.  We *probably* should permanently
        // shut things down, disconnect at least the control pipe.
        throwWinptyException(WINPTY_ERROR_INTERNAL_ERROR,
            L"WriteFile wrote fewer bytes than requested");
    }
}

static void writePacket(winpty_t *wp, WriteBuffer &packet) {
    const auto &buf = packet.buf();
    packet.replaceRawInt32(0, buf.size() - sizeof(int));
    writeData(wp, buf.data(), buf.size());
}

static size_t readData(winpty_t *wp, void *data, size_t amount) {
    DWORD actual = 0;
    OVERLAPPED over = {};
    over.hEvent = wp->ioEvent.get();
    BOOL success = ReadFile(wp->controlPipe.get(), data, amount,
                            &actual, &over);
    if (!success) {
        handlePendingIo(wp, over, success, actual);
        handleReadWriteErrors(wp, success, L"ReadFile failed");
    }
    return actual;
}

static void readAll(winpty_t *wp, void *data, size_t amount) {
    while (amount > 0) {
        size_t chunk = readData(wp, data, amount);
        data = reinterpret_cast<char*>(data) + chunk;
        amount -= chunk;
    }
}

static int32_t readInt32(winpty_t *wp) {
    int32_t ret = 0;
    readAll(wp, &ret, sizeof(ret));
    return ret;
}

// Returns a reply packet's payload.
static ReadBuffer readPacket(winpty_t *wp) {
    int payloadSize = readInt32(wp);
    std::vector<char> bytes(payloadSize);
    readAll(wp, bytes.data(), bytes.size());
    return ReadBuffer(std::move(bytes), ReadBuffer::Throw);
}

static OwnedHandle createControlPipe(const std::wstring &name) {
    const auto sd = createPipeSecurityDescriptorOwnerFullControl();
    if (!sd) {
        throwWinptyException(WINPTY_ERROR_INTERNAL_ERROR,
            L"could not create the control pipe's SECURITY_DESCRIPTOR");
    }
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = sd.get();
    HANDLE ret = CreateNamedPipeW(name.c_str(),
                            /*dwOpenMode=*/
                            PIPE_ACCESS_DUPLEX |
                                FILE_FLAG_FIRST_PIPE_INSTANCE |
                                FILE_FLAG_OVERLAPPED,
                            /*dwPipeMode=*/rejectRemoteClientsPipeFlag(),
                            /*nMaxInstances=*/1,
                            /*nOutBufferSize=*/8192,
                            /*nInBufferSize=*/256,
                            /*nDefaultTimeOut=*/30000,
                            &sa);
    if (ret == INVALID_HANDLE_VALUE) {
        throwLastWindowsError(L"CreateNamedPipeW failed");
    }
    return OwnedHandle(ret);
}

// For debugging purposes, provide a way to keep the console on the main window
// station, visible.
static bool shouldShowConsoleWindow() {
    char buf[32];
    return GetEnvironmentVariableA("WINPTY_SHOW_CONSOLE", buf, sizeof(buf)) > 0;
}

static OwnedHandle startAgentProcess(const std::wstring &desktopName,
                                     const std::wstring &controlPipeName,
                                     DWORD flags, int cols, int rows) {
    const std::wstring exePath = findAgentProgram();
    const std::wstring cmdline =
        (WStringBuilder(256)
            << L"\"" << exePath << L"\" "
            << controlPipeName << L' '
            << flags << L' ' << cols << L' ' << rows).str_moved();

    // Start the agent.
    auto desktopNameM = modifiableWString(desktopName);
    STARTUPINFOW sui = {};
    sui.cb = sizeof(sui);
    if (!desktopName.empty()) {
        sui.lpDesktop = desktopNameM.data();
    }
    if (!shouldShowConsoleWindow()) {
        sui.dwFlags |= STARTF_USESHOWWINDOW;
        sui.wShowWindow = SW_HIDE;
    }
    PROCESS_INFORMATION pi = {};
    auto cmdlineM = modifiableWString(cmdline);
    const bool success =
        CreateProcessW(exePath.c_str(),
                       cmdlineM.data(),
                       nullptr, nullptr,
                       /*bInheritHandles=*/FALSE,
                       /*dwCreationFlags=*/CREATE_NEW_CONSOLE,
                       nullptr, nullptr,
                       &sui, &pi);
    if (!success) {
        const DWORD lastError = GetLastError();
        const auto errStr =
            (WStringBuilder(256)
                << L"winpty-agent CreateProcess failed: cmdline='" << cmdline
                << L"' err=0x" << whexOfInt(lastError)).str_moved();
        trace("%ls", errStr.c_str());
        throwWinptyException(WINPTY_ERROR_AGENT_CREATION_FAILED, errStr);
    }
    CloseHandle(pi.hThread);
    trace("Created agent successfully, pid=%u, cmdline=%ls",
          static_cast<unsigned>(pi.dwProcessId), cmdline.c_str());
    return OwnedHandle(pi.hProcess);
}

WINPTY_API winpty_t *
winpty_open(const winpty_config_t *cfg,
            winpty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        require_arg(cfg != nullptr);

        std::unique_ptr<winpty_t> wp(new winpty_t);
        wp->agentTimeoutMs = cfg->timeoutMs;
        wp->ioEvent = createEvent();

        // Create control server pipe.
        winpty_shared::GenRandom genRandom;
        const auto pipeName =
            L"\\\\.\\pipe\\winpty-control-" + genRandom.uniqueName();
        wp->controlPipe = createControlPipe(pipeName);

        // Create a background desktop.
        // TODO: Respect WINPTY_FLAG_ALLOW_CURPROC_DESKTOP_CREATION.
        BackgroundDesktop desktop;
        if (!shouldShowConsoleWindow()) {
            // TODO: Also, only do this on XP and Vista.
            desktop.create();
        }

        // Start the agent and connect the control pipe.
        wp->agentProcess = startAgentProcess(
            desktop.desktopName(), pipeName, cfg->flags, cfg->cols, cfg->rows);
        connectControlPipe(wp.get());

        // Close handles to the background desktop and restore the original window
        // station.  This must wait until we know the agent is running -- if we
        // close these handles too soon, then the desktop and windowstation will be
        // destroyed before the agent can connect with them.
        desktop.restoreWindowStation();

        // Get the CONIN/CONOUT pipe names.
        auto packet = readPacket(wp.get());
        wp->coninPipeName = packet.getWString();
        wp->conoutPipeName = packet.getWString();
        packet.assertEof();

        return wp.release();
    } API_CATCH(nullptr)
}

WINPTY_API HANDLE winpty_agent_process(winpty_t *wp) {
    return wp == nullptr ? nullptr : wp->agentProcess.get();
}



/*****************************************************************************
 * I/O pipes. */

WINPTY_API LPCWSTR winpty_conin_name(winpty_t *wp) {
    return wp == nullptr ? nullptr : cstrFromWStringOrNull(wp->coninPipeName);
}
WINPTY_API LPCWSTR winpty_conout_name(winpty_t *wp) {
    return wp == nullptr ? nullptr : cstrFromWStringOrNull(wp->conoutPipeName);
}



/*****************************************************************************
 * winpty agent RPC call: process creation. */

WINPTY_API winpty_spawn_config_t *
winpty_spawn_config_new(DWORD winptyFlags,
                        LPCWSTR appname /*OPTIONAL*/,
                        LPCWSTR cmdline /*OPTIONAL*/,
                        LPCWSTR cwd /*OPTIONAL*/,
                        LPCWSTR env /*OPTIONAL*/,
                        winpty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        require_arg((winptyFlags & WINPTY_SPAWN_FLAG_MASK) == winptyFlags);
        std::unique_ptr<winpty_spawn_config_t> cfg(new winpty_spawn_config_t);
        cfg->winptyFlags = winptyFlags;
        if (appname != nullptr) { cfg->appname = appname; }
        if (cmdline != nullptr) { cfg->cmdline = cmdline; }
        if (cwd != nullptr) { cfg->cwd = cwd; }
        if (env != nullptr) {
            const wchar_t *p = env;
            while (*p != L'\0') {
                // Advance over the NUL-terminated string and position 'p'
                // just beyond the string-terminator.
                p += wcslen(p) + 1;
            }
            // Advance over the block-terminator.
            p++;
            cfg->env.assign(env, p);

            // Presumably, an empty Win32 environment would be indicated by a
            // single NUL.  Add an extra NUL just in case we're wrong.
            cfg->env.push_back(L'\0');
        }
        return cfg.release();
    } API_CATCH(nullptr)
}

WINPTY_API void winpty_spawn_config_free(winpty_spawn_config_t *cfg) {
    delete cfg;
}

// I can't find any documentation stating that most relevant Windows handles
// are small integers, which I know them to be.  If Windows HANDLEs actually
// were arbitrary addresses, then DuplicateHandle would be unusable if the
// source process were 64-bits and the caller were 32-bits.  Nevertheless, the
// winpty DLL and the agent are frequently the same architecture, so prefer a
// 64-bit type for maximal robustness.
static inline HANDLE handleFromInt64(int i) {
    return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(i));
}

WINPTY_API BOOL
winpty_spawn(winpty_t *wp,
             const winpty_spawn_config_t *cfg,
             HANDLE *process_handle /*OPTIONAL*/,
             HANDLE *thread_handle /*OPTIONAL*/,
             DWORD *create_process_error /*OPTIONAL*/,
             winpty_error_ptr_t *err /*OPTIONAL*/) {
    if (process_handle != nullptr) { *process_handle = nullptr; }
    if (thread_handle != nullptr) { *thread_handle = nullptr; }
    if (create_process_error != nullptr) { *create_process_error = 0; }
    API_TRY {
        require_arg(wp != nullptr && cfg != nullptr);
        winpty_cxx11::lock_guard<winpty_cxx11::mutex> lock(wp->mutex);

        // Send spawn request.
        WriteBuffer packet;
        packet.putRawInt32(0); // payload size
        packet.putInt32(AgentMsg::StartProcess);
        packet.putInt32(cfg->winptyFlags);
        packet.putInt32(process_handle != nullptr);
        packet.putInt32(thread_handle != nullptr);
        packet.putWString(cfg->appname);
        packet.putWString(cfg->cmdline);
        packet.putWString(cfg->cwd);
        packet.putWString(cfg->env);
        packet.putWString(getDesktopFullName());
        writePacket(wp, packet);

        // Receive reply.
        auto reply = readPacket(wp);
        int status = reply.getInt32();
        DWORD lastError = reply.getInt32();
        HANDLE process = handleFromInt64(reply.getInt64());
        HANDLE thread = handleFromInt64(reply.getInt64());
        reply.assertEof();

        // TODO: Maybe this is good enough, but there are code paths that leak
        // handles...
        if (process_handle != nullptr && process != nullptr) {
            if (!DuplicateHandle(wp->agentProcess.get(), process,
                    GetCurrentProcess(),
                    process_handle, 0, FALSE,
                    DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
                throwLastWindowsError(L"DuplicateHandle of process handle");
            }
        }
        if (thread_handle != nullptr && thread != nullptr) {
            if (!DuplicateHandle(wp->agentProcess.get(), thread,
                    GetCurrentProcess(),
                    thread_handle, 0, FALSE,
                    DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
                throwLastWindowsError(L"DuplicateHandle of thread handle");
            }
        }

        // TODO: error code constants... in AgentMsg.h or winpty.h?
        if (status == 1) {
            // TODO: include an error number
            if (create_process_error != nullptr) {
                *create_process_error = lastError;
            }
            STATIC_ERROR(kError, WINPTY_ERROR_SPAWN_CREATE_PROCESS_FAILED,
                L"CreateProcess failed");
            throwStaticError(kError);
        } else if (status > 1) {
            STATIC_ERROR(kError, WINPTY_ERROR_INTERNAL_ERROR, L"spawn failed");
            throwStaticError(kError);
        }
        return TRUE;
    } API_CATCH(FALSE)
}



/*****************************************************************************
 * winpty agent RPC calls: everything else */

WINPTY_API BOOL
winpty_set_size(winpty_t *wp, int cols, int rows,
                winpty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        require_arg(wp != nullptr && cols > 0 && rows > 0);
        winpty_cxx11::lock_guard<winpty_cxx11::mutex> lock(wp->mutex);
        WriteBuffer packet;
        packet.putRawInt32(0); // payload size
        packet.putInt32(AgentMsg::SetSize);
        packet.putInt32(cols);
        packet.putInt32(rows);
        writePacket(wp, packet);
        readPacket(wp).assertEof();
        return TRUE;
    } API_CATCH(FALSE)
}

WINPTY_API void winpty_free(winpty_t *wp) {
    // At least in principle, CloseHandle can fail, so this deletion can
    // fail.  It won't throw an exception, but maybe there's an error that
    // should be propagated?
    delete wp;
}
