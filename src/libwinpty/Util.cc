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

#include "Util.h"

#include <windows.h>
#include <string.h>

#include <string>

#include "../shared/DebugClient.h"
#include "WinptyException.h"

namespace libwinpty {

void OwnedHandle::dispose(bool nothrow) {
    if (m_h != nullptr && m_h != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(m_h)) {
            trace("CloseHandle(%p) failed", m_h);
            if (!nothrow) {
                throwLastWindowsError(L"CloseHandle failed");
            }
        }
    }
    m_h = nullptr;
}

// This function can throw std::bad_alloc.
wchar_t *dupWStr(const wchar_t *str) {
    const size_t len = wcslen(str) + 1;
    wchar_t *ret = new wchar_t[len];
    memcpy(ret, str, sizeof(wchar_t) * len);
    return ret;
}

// This function can throw std::bad_alloc.
wchar_t *dupWStr(const std::wstring &str) {
    wchar_t *ret = new wchar_t[str.size() + 1];
    str.copy(ret, str.size());
    ret[str.size()] = L'\0';
    return ret;
}

wchar_t *dupWStrOrNull(const std::wstring &str) WINPTY_NOEXCEPT {
    try {
        return dupWStr(str);
    } catch (const std::bad_alloc &e) {
        return nullptr;
    }
}

const wchar_t *cstrFromWStringOrNull(const std::wstring &str) {
    try {
        return str.c_str();
    } catch (const std::bad_alloc &e) {
        return nullptr;
    }
}

void freeWStr(const wchar_t *str) {
    delete [] str;
}

HMODULE getCurrentModule() {
    HMODULE module;
    if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(getCurrentModule),
                &module)) {
        throwLastWindowsError(L"GetModuleHandleExW");
    }
    return module;
}

std::wstring getModuleFileName(HMODULE module) {
    const DWORD bufsize = 4096;
    wchar_t path[bufsize];
    DWORD size = GetModuleFileNameW(module, path, bufsize);
    if (size == 0) {
        throwLastWindowsError(L"GetModuleFileNameW");
    }
    if (size >= bufsize) {
        throwWinptyException(WINPTY_ERROR_INTERNAL_ERROR,
            L"module path is unexpectedly large (at least 4K)");
    }
    return std::wstring(path);
}

std::wstring dirname(const std::wstring &path) {
    std::wstring::size_type pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos)
        return L"";
    else
        return path.substr(0, pos);
}

bool pathExists(const std::wstring &path) {
    return GetFileAttributes(path.c_str()) != 0xFFFFFFFF;
}

OwnedHandle createEvent() {
    HANDLE h = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (h == nullptr) {
        throwLastWindowsError(L"CreateEventW failed");
    }
    return OwnedHandle(h);
}

} // libwinpty namespace
