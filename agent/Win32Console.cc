// Copyright (c) 2011-2012 Ryan Prichard
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

#include "Win32Console.h"
#include "AgentAssert.h"
#include "../shared/DebugClient.h"
#include <string>
#include <wchar.h>
#include <windows.h>

namespace {
    class OsModule {
        HMODULE m_module;
    public:
        OsModule(const wchar_t *fileName) {
            m_module = LoadLibraryW(fileName);
            ASSERT(m_module != NULL);
        }
        ~OsModule() {
            FreeLibrary(m_module);
        }
        HMODULE handle() const { return m_module; }
        FARPROC proc(const char *funcName) {
            FARPROC ret = GetProcAddress(m_module, funcName);
            if (ret == NULL) {
                trace("GetProcAddress: %s is missing", funcName);
            }
            return ret;
        }
    };
}

#define GET_MODULE_PROC(mod, funcName) \
    funcName##Type *p##funcName = reinterpret_cast<funcName##Type*>((mod).proc(#funcName)); \

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

Win32Console::Win32Console() : m_titleWorkBuf(16)
{
    m_conin = GetStdHandle(STD_INPUT_HANDLE);
    m_conout = CreateFileW(L"CONOUT$",
                           GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, 0, NULL);
    ASSERT(m_conout != INVALID_HANDLE_VALUE);
}

Win32Console::~Win32Console()
{
    CloseHandle(m_conout);
}

HANDLE Win32Console::conin()
{
    return m_conin;
}

HANDLE Win32Console::conout()
{
    return m_conout;
}

HWND Win32Console::hwnd()
{
    return GetConsoleWindow();
}

void Win32Console::postCloseMessage()
{
    HWND h = hwnd();
    if (h != NULL)
        PostMessage(h, WM_CLOSE, 0, 0);
}

// A Windows console window can never be larger than the desktop window.  To
// maximize the possible size of the console in rows*cols, try to configure
// the console with a small font.  Unfortunately, we cannot make the font *too*
// small, because there is also a minimum window size in pixels.
void Win32Console::setSmallFont()
{
    // I measured the minimum window size in Windows 7 and Windows 10 VMs on
    // a typical-DPI monitor.  The minimum row count was always 1, but the
    // minimum width varied.
    //
    // XXX: High-DPI monitors might break this code.  The minimum console
    // window might be much larger when measured in pixels.
    //
    // XXX: We could resize the console font when the terminal is resized.

    // Lucida Console 6px: cells are 4x6 pixels.  Minimum window is 24x1.
    if (setConsoleFont(L"Lucida Console", 6))
        return;
    // Consolas 8px: cells are 4x8 pixels.  Minimum window is 24x1.
    if (setConsoleFont(L"Consolas", 8))
        return;
    // Smallest raster font is 4x6.  Minimum window is 24x1.
    if (setSmallConsoleFontVista())
        return;
    if (setSmallConsoleFontXP())
        return;
    trace("Error: could not configure console font -- continuing anyway");
    dumpConsoleFont("setSmallFont: final font: ");
}

// Some of these types and functions are missing from the MinGW headers.
// Others are undocumented.

struct AGENT_CONSOLE_FONT_INFO {
    DWORD nFont;
    COORD dwFontSize;
};

struct AGENT_CONSOLE_FONT_INFOEX {
    ULONG cbSize;
    DWORD nFont;
    COORD dwFontSize;
    UINT FontFamily;
    UINT FontWeight;
    WCHAR FaceName[LF_FACESIZE];
};

// undocumented XP API
typedef BOOL WINAPI SetConsoleFontType(
            HANDLE hOutput,
            DWORD dwFontIndex);

// XP and up
typedef BOOL WINAPI GetCurrentConsoleFontType(
            HANDLE hOutput,
            BOOL bMaximize,
            AGENT_CONSOLE_FONT_INFO *pFontInfo);

// XP and up
typedef COORD WINAPI GetConsoleFontSizeType(
            HANDLE hConsoleOutput,
            DWORD nFont);

// Vista and up
typedef BOOL WINAPI GetCurrentConsoleFontExType(
            HANDLE hConsoleOutput,
            BOOL bMaximumWindow,
            AGENT_CONSOLE_FONT_INFOEX *lpConsoleCurrentFontEx);

// Vista and up
typedef BOOL WINAPI SetCurrentConsoleFontExType(
            HANDLE hConsoleOutput,
            BOOL bMaximumWindow,
            AGENT_CONSOLE_FONT_INFOEX *lpConsoleCurrentFontEx);

// Attempt to set the console font to the given facename and pixel size.
// These APIs should exist on Vista and up.
bool Win32Console::setConsoleFont(const wchar_t *faceName, int pixelSize)
{
    trace("setConsoleFont: attempting to set console font to %dpx %ls", pixelSize, faceName);

    OsModule dll(L"kernel32.dll");
    GET_MODULE_PROC(dll, GetCurrentConsoleFontEx);
    GET_MODULE_PROC(dll, SetCurrentConsoleFontEx);
    if (pGetCurrentConsoleFontEx == NULL || pSetCurrentConsoleFontEx == NULL) {
        trace("setConsoleFont failed: missing API(s)");
        return false;
    }
    dumpConsoleFont("setConsoleFont: cur font: ");

    AGENT_CONSOLE_FONT_INFOEX fontex = {0};
    fontex.cbSize = sizeof(fontex);
    fontex.FontWeight = 400;
    fontex.dwFontSize.Y = pixelSize;
    wcsncpy(fontex.FaceName, faceName, COUNT_OF(fontex.FaceName));
    if (!pSetCurrentConsoleFontEx(m_conout, FALSE, &fontex)) {
        trace("setConsoleFont failed: SetCurrentConsoleFontEx call failed");
        return false;
    }

    memset(&fontex, 0, sizeof(fontex));
    fontex.cbSize = sizeof(fontex);
    if (!pGetCurrentConsoleFontEx(m_conout, FALSE, &fontex)) {
        trace("setConsoleFont failed: GetCurrentConsoleFontEx call failed");
        return false;
    }
    if (wcsncmp(fontex.FaceName, faceName, COUNT_OF(fontex.FaceName)) != 0) {
        wchar_t curFace[COUNT_OF(fontex.FaceName) + 1];
        wcsncpy(curFace, fontex.FaceName, COUNT_OF(curFace));
        trace("setConsoleFont failed: new facename is %ls", curFace);
        return false;
    }
    // XXX: Will the post-call font size always have *exactly* the same Y
    // value?
    if (fontex.dwFontSize.Y != pixelSize) {
        trace("setConsoleFont failed: new font size is %d,%d",
              fontex.dwFontSize.X, fontex.dwFontSize.Y);
        return false;
    }

    trace("setConsoleFont succeeded");
    dumpConsoleFont("setConsoleFont: final font: ");
    return true;
}

// Attempt to set the console font using the size of the 0-index font.  This
// seems to select a raster font.
//
// Perhaps this behavior should be removed in favor of assuming the computer
// has one of the hard-coded fonts?
//
bool Win32Console::setSmallConsoleFontVista()
{
    trace("setSmallConsoleFontVista was called");

    OsModule dll(L"kernel32.dll");
    GET_MODULE_PROC(dll, GetConsoleFontSize);
    GET_MODULE_PROC(dll, SetCurrentConsoleFontEx);
    if (pGetConsoleFontSize == NULL || pSetCurrentConsoleFontEx == NULL) {
        trace("setSmallConsoleFontVista failed: missing API(s)");
        return false;
    }
    dumpConsoleFont("setSmallConsoleFontVista: cur font: ");
    COORD smallest = pGetConsoleFontSize(m_conout, 0);
    trace("setSmallConsoleFontVista: smallest=%d,%d", smallest.X, smallest.Y);
    if (smallest.X == 0 || smallest.Y == 0) {
        trace("setSmallConsoleFontVista failed: GetConsoleFontSize call failed");
        return false;
    }
    AGENT_CONSOLE_FONT_INFOEX fontex = {0};
    fontex.cbSize = sizeof(fontex);
    fontex.nFont = 0;
    fontex.dwFontSize = smallest;
    if (!pSetCurrentConsoleFontEx(m_conout, FALSE, &fontex)) {
        trace("setSmallConsoleFontVista failed: SetCurrentConsoleFontEx call failed");
        return false;
    }

    trace("setSmallConsoleFontVista succeeded");
    dumpConsoleFont("setSmallConsoleFontVista: final font: ");
    return true;
}

// Use undocumented APIs to set a small console font on XP.
//
// Somewhat described here:
// http://blogs.microsoft.co.il/blogs/pavely/archive/2009/07/23/changing-console-fonts.aspx
//
bool Win32Console::setSmallConsoleFontXP()
{
    trace("setSmallConsoleFontXP: attempting to use undocumented XP API");

    OsModule dll(L"kernel32.dll");
    GET_MODULE_PROC(dll, SetConsoleFont);
    if (pSetConsoleFont == NULL) {
        trace("setSmallConsoleFontXP failed: missing API");
        return false;
    }

    // The undocumented GetNumberOfConsoleFonts API reports that my Windows 7
    // system has 12 fonts on it.  Each font is really just a differently-sized
    // raster/Terminal font.  Font index 0 is the smallest font, so we want to
    // choose it.

    dumpConsoleFont("setSmallConsoleFontXP: cur font: ");
    if (!pSetConsoleFont(m_conout, 0)) {
        trace("setSmallConsoleFontXP failed: SetConsoleFont failed");
        return false;
    }

    trace("setSmallConsoleFontXP succeeded");
    dumpConsoleFont("setSmallConsoleFontXP: final font: ");
    return true;
}

void Win32Console::dumpConsoleFont(const char *prefix)
{
    OsModule dll(L"kernel32.dll");

    GET_MODULE_PROC(dll, GetCurrentConsoleFontEx);
    if (pGetCurrentConsoleFontEx != NULL) {
        AGENT_CONSOLE_FONT_INFOEX fontex = {0};
        fontex.cbSize = sizeof(fontex);
        if (!pGetCurrentConsoleFontEx(m_conout, FALSE, &fontex)) {
            trace("GetCurrentConsoleFontEx call failed");
            return;
        }
        wchar_t curFace[COUNT_OF(fontex.FaceName) + 1];
        wcsncpy(curFace, fontex.FaceName, COUNT_OF(curFace));
        trace("%sfontex.nFont=%d", prefix, fontex.nFont);
        trace("%sfontex.dwFontSize=%d,%d",
              prefix, fontex.dwFontSize.X, fontex.dwFontSize.Y);
        trace("%sfontex.FontFamily=%d", prefix, fontex.FontFamily);
        trace("%sfontex.FontWeight=%d", prefix, fontex.FontWeight);
        trace("%sfontex.FaceName=%ls", prefix, curFace);
        return;
    }

    GET_MODULE_PROC(dll, GetCurrentConsoleFont);
    if (pGetCurrentConsoleFont != NULL) {
        AGENT_CONSOLE_FONT_INFO font = {0};
        pGetCurrentConsoleFont(m_conout, FALSE, &font);
        trace("%sfont.nFont=%d", prefix, font.nFont);
        trace("%sfont.dwFontSize=%d,%d",
              prefix, font.dwFontSize.X, font.dwFontSize.Y);
    }
}

void Win32Console::clearLines(
    int row,
    int count,
    const ConsoleScreenBufferInfo &info)
{
    // TODO: error handling
    const int width = SmallRect(info.srWindow).width();
    DWORD actual = 0;
    if (!FillConsoleOutputCharacterW(
            m_conout, L' ', width * count, Coord(0, row),
            &actual) || static_cast<int>(actual) != width * count) {
        trace("FillConsoleOutputCharacterW failed");
    }
    if (!FillConsoleOutputAttribute(
            m_conout, info.wAttributes, width * count, Coord(0, row),
            &actual) || static_cast<int>(actual) != width * count) {
        trace("FillConsoleOutputAttribute failed");
    }
}

ConsoleScreenBufferInfo Win32Console::bufferInfo()
{
    // TODO: error handling
    ConsoleScreenBufferInfo info;
    if (!GetConsoleScreenBufferInfo(m_conout, &info)) {
        trace("GetConsoleScreenBufferInfo failed");
    }
    return info;
}

Coord Win32Console::bufferSize()
{
    return bufferInfo().bufferSize();
}

SmallRect Win32Console::windowRect()
{
    return bufferInfo().windowRect();
}

void Win32Console::resizeBuffer(const Coord &size)
{
    // TODO: error handling
    if (!SetConsoleScreenBufferSize(m_conout, size)) {
        trace("SetConsoleScreenBufferSize failed");
    }
}

void Win32Console::moveWindow(const SmallRect &rect)
{
    // TODO: error handling
    if (!SetConsoleWindowInfo(m_conout, TRUE, &rect)) {
        trace("SetConsoleWindowInfo failed");
    }
}

Coord Win32Console::cursorPosition()
{
    return bufferInfo().dwCursorPosition;
}

void Win32Console::setCursorPosition(const Coord &coord)
{
    // TODO: error handling
    if (!SetConsoleCursorPosition(m_conout, coord)) {
        trace("SetConsoleCursorPosition failed");
    }
}

void Win32Console::writeInput(const INPUT_RECORD *ir, int count)
{
    // TODO: error handling
    DWORD dummy = 0;
    if (!WriteConsoleInput(m_conin, ir, count, &dummy)) {
        trace("WriteConsoleInput failed");
    }
}

bool Win32Console::processedInputMode()
{
    // TODO: error handling
    DWORD mode = 0;
    if (!GetConsoleMode(m_conin, &mode)) {
        trace("GetConsoleMode failed");
    }
    return (mode & ENABLE_PROCESSED_INPUT) == ENABLE_PROCESSED_INPUT;
}

void Win32Console::read(const SmallRect &rect, CHAR_INFO *data)
{
    // TODO: error handling
    SmallRect tmp(rect);
    if (!ReadConsoleOutputW(m_conout, data, rect.size(), Coord(), &tmp)) {
        trace("ReadConsoleOutput failed [x:%d,y:%d,w:%d,h:%d]",
              rect.Left, rect.Top, rect.width(), rect.height());
    }
}

void Win32Console::write(const SmallRect &rect, const CHAR_INFO *data)
{
    // TODO: error handling
    SmallRect tmp(rect);
    if (!WriteConsoleOutputW(m_conout, data, rect.size(), Coord(), &tmp)) {
        trace("WriteConsoleOutput failed");
    }
}

std::wstring Win32Console::title()
{
    while (true) {
        // The MSDN documentation for GetConsoleTitle is wrong.  It documents
        // nSize as the "size of the buffer pointed to by the lpConsoleTitle
        // parameter, in characters" and the successful return value as "the
        // length of the console window's title, in characters."  In fact,
        // nSize is in *bytes*.  In contrast, the return value is a count of
        // UTF-16 code units.  Make the buffer extra large so we can
        // technically match the documentation.
        DWORD count = GetConsoleTitleW(m_titleWorkBuf.data(),
                                       m_titleWorkBuf.size());
        if (count >= m_titleWorkBuf.size() / sizeof(wchar_t)) {
            m_titleWorkBuf.resize((count + 1) * sizeof(wchar_t));
            continue;
        }
        m_titleWorkBuf[count] = L'\0';
        return m_titleWorkBuf.data();
    }
}

void Win32Console::setTitle(const std::wstring &title)
{
    if (!SetConsoleTitleW(title.c_str())) {
        trace("SetConsoleTitleW failed");
    }
}
