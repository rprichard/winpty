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

// Recent 4.x MinGW and MinGW-w64 gcc compilers lack std::mutex and
// std::lock_guard.  I have a 5.2.0 MinGW-w64 compiler packaged through MSYS2
// that *is* new enough, but that's one compiler against several deficient
// ones.  Wrap CRITICAL_SECTION instead.

#ifndef WINPTY_CXX11_MUTEX_H
#define WINPTY_CXX11_MUTEX_H

#include <windows.h>

namespace winpty_cxx11 {

class mutex {
    CRITICAL_SECTION m_mutex;
public:
    mutex()         { InitializeCriticalSection(&m_mutex);  }
    ~mutex()        { DeleteCriticalSection(&m_mutex);      }
    void lock()     { EnterCriticalSection(&m_mutex);       }
    void unlock()   { LeaveCriticalSection(&m_mutex);       }

    mutex(const mutex &other) = delete;
    mutex &operator=(const mutex &other) = delete;
};

template <typename T>
class lock_guard {
    T &m_lock;
public:
    lock_guard(T &lock) : m_lock(lock)  { m_lock.lock();    }
    ~lock_guard()                       { m_lock.unlock();  }

    lock_guard(const lock_guard &other) = delete;
    lock_guard &operator=(const lock_guard &other) = delete;
};

} // winpty_cxx11 namespace

#endif // WINPTY_CXX11_MUTEX_H
