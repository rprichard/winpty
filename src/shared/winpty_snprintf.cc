// Copyright (c) 2012-2016 Ryan Prichard
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

// To work around a MinGW bug, we must call these functions from a separate
// translation unit and carefully control the order that system headers are
// included.  This works:
//
//    #include <stdio.h>
//    #include <string>
//
// This does not:
//
//    #include <string>
//    #include <stdio.h>
//
// With the latter, _vscwprintf is not declared.  This bug affects MinGW's
// official distribution, but not Cygwin's MinGW distribution.  It also does
// not affect MinGW-w64.

#include <windows.h>
#include <stdio.h>
#include <wchar.h>

#include "winpty_snprintf.h"

#include <assert.h>

bool winpty_vsnprintf(char *out, size_t size,
                      const char *format, va_list ap) {
    assert(out != NULL && size != 0);
    out[0] = '\0';
#if defined(__CYGWIN__) || defined(__MSYS__)
    const int count = vsnprintf(out, size, format, ap);
#elif defined(_MSC_VER)
    const int count = _vsnprintf_s(out, size, _TRUNCATE, format, ap);
#else
    const int count = _vsnprintf(out, size, format, ap);
#endif
    out[size - 1] = '\0';
    return count >= 0 && static_cast<size_t>(count) < size;
}

#if !defined(__CYGWIN__) && !defined(__MSYS__)

bool winpty_vswprintf(wchar_t *out, size_t size,
                      const wchar_t *format, va_list ap) {
    assert(out != NULL && size != 0);
    out[0] = L'\0';
#if defined(_MSC_VER)
    const int count = _vsnwprintf_s(out, size, _TRUNCATE, format, ap);
#else
    const int count = _vsnwprintf(out, size, format, ap);
#endif
    out[size - 1] = L'\0';
    return count >= 0 && static_cast<size_t>(count) < size;
}

int winpty_vscprintf(const char *format, va_list ap) {
    return _vscprintf(format, ap);
}

int winpty_vscwprintf(const wchar_t *format, va_list ap) {
    return _vscwprintf(format, ap);
}

#endif // !defined(__CYGWIN__) && !defined(__MSYS__)
