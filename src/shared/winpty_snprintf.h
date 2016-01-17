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

// MSVC did not define an snprintf function prior to MSVC 2015, and the
// snprintf in MSVC 2015 is undocumented as of this writing (Jan 2016).  This
// header defines portable alternatives.  Specifically, this header defines
// winpty_snprintf and winpty_vsnprintf.
//
// These two functions are not C99-conformant, because C99 conformance is
// difficult to achieve.  Details:
//
//  - On Cygwin, the format string is GNU/ISO-C style.  (e.g. PRIx64 is 'llx',
//    and Cygwin's printf does not recognize '%I64x' at all.)
//
//  - On native Windows (MinGW/MinGW-w64/MSVC), the format string is Microsoft
//    style.  e.g. PRIx64 is 'I64x'.  'llx' tends to work on Vista and up, but
//    it is truncated on XP.  'z' for size_t does not work until MSVC 2015.
//
//  - Do not use inttypes.h.  If MinGW-w64 is configured with
//    -D__USE_MINGW_ANSI_STDIO=1, then its PRIx64 changes to 'll', but this
//    header is still bypassing MinGW overrides and calling MSVCRT.DLL
//    directly, and 'll' breaks on XP.  MinGW doesn't change its PRIx64.  Use
//    WINPTY_PRI??? instead.
//
//  - The winpty_[v]s[nw]printf functions returns true/false for
//    success/failure.  If the buffer is too small, the function writes as many
//    bytes as possible and returns false.  The buffer will always be
//    NUL-terminated on return.
//
//  - The winpty_[v]c[nw]printf functions are only implemented on native
//    Windows.  AFAICT, it is actually impossible to implement the wide-string
//    version of these functions in standard C and on Linux.  The
//    winpty_[v]swprintf functions are only implemented on native Windows,
//    because, even though Cygwin has vswprintf, MSYS1 does not.

#ifndef WINPTY_SNPRINTF_H
#define WINPTY_SNPRINTF_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

// 64-bit format strings.

#if defined(__CYGWIN__) || defined(__MSYS__)
#define WINPTY_PRId64 "lld"
#define WINPTY_PRIu64 "llu"
#define WINPTY_PRIx64 "llx"
#define WINPTY_PRIX64 "llX"
#else
#define WINPTY_PRId64 "I64d"
#define WINPTY_PRIu64 "I64u"
#define WINPTY_PRIx64 "I64x"
#define WINPTY_PRIX64 "I64X"
#endif

// Format checking declarations

// The GCC printf checking only works with narrow strings.  It prints the error
// "error: format string argument is not a string type" for the wide functions.
#if defined(__CYGWIN__) || defined(__MSYS__)
#define WINPTY_SNPRINTF_FORMAT       __attribute__((format(printf, 3, 4)))
#define WINPTY_SNPRINTF_ARRAY_FORMAT __attribute__((format(printf, 2, 3)))
#elif defined(__GNUC__)
#define WINPTY_SNPRINTF_FORMAT       __attribute__((format(ms_printf, 3, 4)))
#define WINPTY_SNPRINTF_ARRAY_FORMAT __attribute__((format(ms_printf, 2, 3)))
#define WINPTY_SCPRINTF_FORMAT       __attribute__((format(ms_printf, 1, 2)))
#else
#define WINPTY_SNPRINTF_FORMAT
#define WINPTY_SNPRINTF_ARRAY_FORMAT
#define WINPTY_SCPRINTF_FORMAT
#endif

inline bool winpty_snprintf(char *out, size_t size,
                            const char *format, ...) WINPTY_SNPRINTF_FORMAT;

template <size_t size> bool winpty_snprintf(char (&out)[size],
                                            const char *format, ...)
                                            WINPTY_SNPRINTF_ARRAY_FORMAT;

// Inline definitions

#define WINPTY_SNPRINTF_FORWARD(type, expr) \
    do {                                    \
        va_list ap;                         \
        va_start(ap, format);               \
        type temp_ = (expr);                \
        va_end(ap);                         \
        return temp_;                       \
    } while (false)

bool winpty_vsnprintf(char *out, size_t size,
                      const char *format, va_list ap);

template <size_t size> bool winpty_vsnprintf(
        char (&out)[size],
        const char *format, va_list ap) {
    return winpty_vsnprintf(out, size, format, ap);
}

inline bool winpty_snprintf(char *out, size_t size,
                            const char *format, ...) {
    WINPTY_SNPRINTF_FORWARD(bool, winpty_vsnprintf(out, size, format, ap));
}

template <size_t size> bool winpty_snprintf(char (&out)[size],
                                            const char *format, ...) {
    WINPTY_SNPRINTF_FORWARD(bool, winpty_vsnprintf(out, size, format, ap));
}

#if !defined(__CYGWIN__) && !defined(__MSYS__)

bool winpty_vswprintf(wchar_t *out, size_t size,
                      const wchar_t *format, va_list ap);

template <size_t size> bool winpty_vswprintf(
        wchar_t (&out)[size],
        const wchar_t *format, va_list ap) {
    return winpty_vswprintf(out, size, format, ap);
}

inline bool winpty_swprintf(wchar_t *out, size_t size,
                            const wchar_t *format, ...) {
    WINPTY_SNPRINTF_FORWARD(bool, winpty_vswprintf(out, size, format, ap));
}

template <size_t size> bool winpty_swprintf(wchar_t (&out)[size],
                                            const wchar_t *format, ...) {
    WINPTY_SNPRINTF_FORWARD(bool, winpty_vswprintf(out, size, format, ap));
}

int winpty_vscprintf(const char *format, va_list ap);
int winpty_vscwprintf(const wchar_t *format, va_list ap);

inline int winpty_scprintf(const char *format, ...) WINPTY_SCPRINTF_FORMAT;
inline int winpty_scprintf(const char *format, ...) {
    WINPTY_SNPRINTF_FORWARD(int, winpty_vscprintf(format, ap));
}

inline int winpty_scwprintf(const wchar_t *format, ...) {
    WINPTY_SNPRINTF_FORWARD(int, winpty_vscwprintf(format, ap));
}

#endif // !defined(__CYGWIN__) && !defined(__MSYS__)

#endif // WINPTY_SNPRINTF_H
