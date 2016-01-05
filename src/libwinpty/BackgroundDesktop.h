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

#ifndef LIBWINPTY_BACKGROUND_DESKTOP_H
#define LIBWINPTY_BACKGROUND_DESKTOP_H

#include <windows.h>

#include <string>

namespace libwinpty {

class BackgroundDesktop {
    bool m_created = false;
    HWINSTA m_originalStation = nullptr;
    HWINSTA m_newStation = nullptr;
    HDESK m_newDesktop = nullptr;
    std::wstring m_newDesktopName;
public:
    void create();
    void restoreWindowStation(bool nothrow=false);
    const std::wstring &desktopName() const { return m_newDesktopName; }
    BackgroundDesktop() {}
    ~BackgroundDesktop();
    // No copy ctor/assignment
    BackgroundDesktop(const BackgroundDesktop &other) = delete;
    BackgroundDesktop &operator=(const BackgroundDesktop &other) = delete;
};

std::wstring getDesktopFullName();

} // libwinpty namespace

#endif // LIBWINPTY_BACKGROUND_DESKTOP_H
