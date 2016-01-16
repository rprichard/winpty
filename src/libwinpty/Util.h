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

#ifndef LIBWINPTY_UTIL_H
#define LIBWINPTY_UTIL_H

#include <string>
#include <utility>
#include <vector>

#include "../include/winpty.h"
#include "../shared/cxx11_noexcept.h"

namespace libwinpty {

class OwnedHandle {
    HANDLE m_h;
public:
    OwnedHandle() : m_h(nullptr) {}
    OwnedHandle(HANDLE h) : m_h(h) {}
    ~OwnedHandle() { dispose(true); }
    void dispose(bool nothrow=false);
    HANDLE get() const { return m_h; }
    HANDLE release() { HANDLE ret = m_h; m_h = nullptr; return ret; }
    OwnedHandle(const OwnedHandle &other) = delete;
    OwnedHandle(OwnedHandle &&other) : m_h(other.release()) {}
    OwnedHandle &operator=(const OwnedHandle &other) = delete;
    OwnedHandle &operator=(OwnedHandle &&other) {
        dispose();
        m_h = other.release();
        return *this;
    }
};

// Once an I/O operation fails with ERROR_IO_PENDING, the caller *must* wait
// for it to complete, even after calling CancelIo on it!  See
// https://blogs.msdn.microsoft.com/oldnewthing/20110202-00/?p=11613.  This
// class enforces that requirement.
class PendingIo {
    HANDLE m_file;
    OVERLAPPED &m_over;
    bool m_finished;
public:
    // The file handle and OVERLAPPED object must live as long as the PendingIo
    // object.
    PendingIo(HANDLE file, OVERLAPPED &over) :
        m_file(file), m_over(over), m_finished(false) {}
    ~PendingIo() {
        if (!m_finished) {
            // We're not usually that interested in CancelIo's return value.
            // In any case, we must not throw an exception in this dtor.
            CancelIo(&m_over);
            waitForCompletion();
        }
    }
    BOOL waitForCompletion(DWORD &actual) WINPTY_NOEXCEPT {
        m_finished = true;
        return GetOverlappedResult(m_file, &m_over, &actual, TRUE);
    }
    BOOL waitForCompletion() WINPTY_NOEXCEPT {
        DWORD actual = 0;
        return waitForCompletion(actual);
    }
};

inline std::vector<wchar_t> modifiableWString(const std::wstring &str) {
    std::vector<wchar_t> ret(str.size() + 1);
    str.copy(ret.data(), str.size());
    ret[str.size()] = L'\0';
    return ret;
}

wchar_t *dupWStr(const std::wstring &str);
wchar_t *dupWStr(const wchar_t *str);
wchar_t *dupWStrOrNull(const std::wstring &str) WINPTY_NOEXCEPT;
const wchar_t *cstrFromWStringOrNull(const std::wstring &str);
void freeWStr(const wchar_t *str);

HMODULE getCurrentModule();
std::wstring getModuleFileName(HMODULE module);
std::wstring dirname(const std::wstring &path);
bool pathExists(const std::wstring &path);

OwnedHandle createEvent();

} // libwinpty namespace

#endif // LIBWINPTY_UTIL_H
