// Copyright (c) 2015 Ryan Prichard
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

#include "WinptyException.h"

#include <memory>

#include "Util.h"
#include "WinptyInternal.h"

namespace libwinpty {

EXTERN_ERROR(kOutOfMemory,
    WINPTY_ERROR_OUT_OF_MEMORY,
    L"out of memory"
);

// This function can throw std::bad_alloc.
WinptyException::WinptyException(winpty_result_t code, const std::wstring &msg) {
    std::unique_ptr<winpty_error_t> error(new winpty_error_t);
    error->errorIsStatic = false;
    error->code = code;
    error->msg = dupWStr(msg);
    m_error = error.release();
}

WinptyException::WinptyException(const WinptyException &other) WINPTY_NOEXCEPT {
    if (other.m_error->errorIsStatic) {
        m_error = other.m_error;
    } else {
        try {
            std::unique_ptr<winpty_error_t> error(new winpty_error_t);
            error->errorIsStatic = false;
            error->code = other.m_error->code;
            error->msg = dupWStr(other.m_error->msg);
            m_error = error.release();
        } catch (const std::bad_alloc &e) {
            m_error = const_cast<winpty_error_ptr_t>(&kOutOfMemory);
        }
    }
}

// Throw a statically-allocated winpty_error_t object.
void throwStaticError(const winpty_error_t &error) {
    throw WinptyException(const_cast<winpty_error_ptr_t>(&error));
}

void throwWinptyException(winpty_result_t code, const std::wstring &msg) {
    throw WinptyException(code, msg);
}

// Code size optimization -- callers don't have to make an std::wstring.
void throwWinptyException(winpty_result_t code, const wchar_t *msg) {
    throw WinptyException(code, msg);
}

void throwWindowsError(const std::wstring &prefix, DWORD error) {
    wchar_t msg[64];
    wsprintf(msg, L": error %u", static_cast<unsigned int>(error));
    throwWinptyException(WINPTY_ERROR_WINDOWS_ERROR,
                         prefix + msg);
}

void throwLastWindowsError(const std::wstring &prefix) {
    throwWindowsError(prefix, GetLastError());
}

// Code size optimization -- callers don't have to make an std::wstring.
void throwLastWindowsError(const wchar_t *prefix) {
    throwWindowsError(prefix, GetLastError());
}

} // libwinpty namespace
