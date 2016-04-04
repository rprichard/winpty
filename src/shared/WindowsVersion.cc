// Copyright (c) 2016 Ryan Prichard
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

#include "WindowsVersion.h"

#include <windows.h>

#include <tuple>

#include "WinptyAssert.h"

namespace {

typedef std::tuple<DWORD, DWORD> Version;

// This function can only return a version up to 6.2 unless the executable is
// manifested for a newer version of Windows.  See the MSDN documentation for
// GetVersionEx.
Version getWindowsVersion() {
    // Allow use of deprecated functions (i.e. GetVersionEx).  We need to use
    // GetVersionEx for the old MinGW toolchain and with MSVC when it targets XP.
    // Having two code paths makes code harder to test, and it's not obvious how
    // to detect the presence of a new enough SDK.  (Including ntverp.h and
    // examining VER_PRODUCTBUILD apparently works, but even then, MinGW-w64 and
    // MSVC seem to use different version numbers.)
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif
    OSVERSIONINFO info = {};
    info.dwOSVersionInfoSize = sizeof(info);
    const auto success = GetVersionEx(&info);
    ASSERT(success && "GetVersionEx failed");
    return Version(info.dwMajorVersion, info.dwMinorVersion);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}

} // anonymous namespace

// Returns true for Windows Vista (or Windows Server 2008) or newer.
bool isAtLeastWindowsVista() {
    return getWindowsVersion() >= Version(6, 0);
}

// Returns true for Windows 8 (or Windows Server 2012) or newer.
bool isAtLeastWindows8() {
    return getWindowsVersion() >= Version(6, 2);
}
