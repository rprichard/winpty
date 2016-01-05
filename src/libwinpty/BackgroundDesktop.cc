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

#include "BackgroundDesktop.h"

#include <windows.h>
#include <assert.h>

#include "../shared/DebugClient.h"
#include "Util.h"
#include "WinptyException.h"

namespace libwinpty {

static std::wstring getObjectName(HANDLE object) {
    // TODO: Update for error robustness -- it can assert-fail a bit too
    // easily, but it's independent of everything else.
    BOOL success;
    DWORD lengthNeeded = 0;
    GetUserObjectInformationW(object, UOI_NAME,
                              nullptr, 0,
                              &lengthNeeded);
    assert(lengthNeeded % sizeof(wchar_t) == 0);
    wchar_t *tmp = new wchar_t[lengthNeeded / 2];
    success = GetUserObjectInformationW(object, UOI_NAME,
                                        tmp, lengthNeeded,
                                        nullptr);
    assert(success && "GetUserObjectInformationW failed");
    std::wstring ret = tmp;
    delete [] tmp;
    return ret;
}

// Get a non-interactive window station for the agent.  Failure is mostly
// acceptable; we'll just use the normal window station and desktop.  That
// apparently happens in some contexts such as SSH logins.
// TODO: review security w.r.t. windowstation and desktop.
void BackgroundDesktop::create() {
    // create() should be called at most once.
    auto fail = [](const char *func) {
        trace("%s failed - using normal window station and desktop", func);
    };
    assert(!m_created && "BackgroundDesktop::create called twice");
    m_created = true;
    m_newStation =
        CreateWindowStationW(nullptr, 0, WINSTA_ALL_ACCESS, nullptr);
    if (m_newStation == nullptr) {
        fail("CreateWindowStationW");
        return;
    }
    HWINSTA originalStation = GetProcessWindowStation();
    if (originalStation == nullptr) {
        fail("GetProcessWindowStation");
        return;
    }
    if (!SetProcessWindowStation(m_newStation)) {
        fail("SetProcessWindowStation");
        return;
    }
    // Record the original station so that it will be restored.
    m_originalStation = originalStation;
    m_newDesktop =
        CreateDesktopW(L"Default", nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    if (m_newDesktop == nullptr) {
        fail("CreateDesktopW");
        return;
    }
    m_newDesktopName =
        getObjectName(m_newStation) + L"\\" + getObjectName(m_newDesktop);
}

void BackgroundDesktop::restoreWindowStation(bool nothrow) {
    if (m_originalStation != nullptr) {
        if (!SetProcessWindowStation(m_originalStation)) {
            trace("could not restore window station");
            if (!nothrow) {
                throwLastWindowsError(L"SetProcessWindowStation failed");
            }
        }
        m_originalStation = nullptr;
    }
}

BackgroundDesktop::~BackgroundDesktop() {
    restoreWindowStation(true);
    if (m_newDesktop != nullptr) { CloseDesktop(m_newDesktop); }
    if (m_newStation != nullptr) { CloseWindowStation(m_newStation); }
}

std::wstring getDesktopFullName() {
    // TODO: This function should throw windows exceptions...
    // MSDN says that the handle returned by GetThreadDesktop does not need
    // to be passed to CloseDesktop.
    HWINSTA station = GetProcessWindowStation();
    HDESK desktop = GetThreadDesktop(GetCurrentThreadId());
    assert(station != nullptr && "GetProcessWindowStation returned NULL");
    assert(desktop != nullptr && "GetThreadDesktop returned NULL");
    return getObjectName(station) + L"\\" + getObjectName(desktop);
}

} // libwinpty namespace
