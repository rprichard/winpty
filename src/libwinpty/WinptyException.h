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

#ifndef LIBWINPTY_WINPTY_EXCEPTION_H
#define LIBWINPTY_WINPTY_EXCEPTION_H

#include <windows.h>

#include <string>

#include "../include/winpty.h"
#include "../shared/cxx11_noexcept.h"

namespace libwinpty {

#define STATIC_ERROR(name, code, msg) \
    static const winpty_error_t name = { true, (code), (msg) };

#define EXTERN_ERROR(name, code, msg) \
    extern const winpty_error_t name = { true, (code), (msg) };

extern const winpty_error_t kOutOfMemory;

class WinptyException {
    winpty_error_ptr_t m_error;

public:
    WinptyException(winpty_result_t code, const std::wstring &msg);
    WinptyException(winpty_error_ptr_t error) : m_error(error) {}
    ~WinptyException() {
        if (m_error != nullptr) {
            winpty_error_free(m_error);
        }
    }

    winpty_error_ptr_t release() WINPTY_NOEXCEPT {
        winpty_error_ptr_t ret = m_error;
        m_error = nullptr;
        return ret;
    }

    WinptyException &operator=(const WinptyException &other) = delete;
    WinptyException &operator=(WinptyException &&other) = delete;

    WinptyException(const WinptyException &other) WINPTY_NOEXCEPT;
    WinptyException(WinptyException &&other) WINPTY_NOEXCEPT
        : m_error(other.release()) {}
};

void throwStaticError(const winpty_error_t &error);
void throwWinptyException(winpty_result_t code, const std::wstring &msg);
void throwWinptyException(winpty_result_t code, const wchar_t *msg);
void throwLastWindowsError(const std::wstring &prefix);
void throwLastWindowsError(const wchar_t *prefix);

} // libwinpty namespace

#endif // LIBWINPTY_WINPTY_EXCEPTION_H
