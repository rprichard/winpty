#include "Win32Console.h"
#include "AgentAssert.h"
#include "../Shared/DebugClient.h"
#include <windows.h>

Win32Console::Win32Console()
{
    m_conin = CreateFile(
                L"CONIN$",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_EXISTING, 0, NULL);
    m_conout = CreateFile(
                L"CONOUT$",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_EXISTING, 0, NULL);
    ASSERT(m_conin != NULL);
    ASSERT(m_conout != NULL);
}

Win32Console::~Win32Console()
{
    CloseHandle(m_conin);
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

Coord Win32Console::bufferSize()
{
    // TODO: error handling
    CONSOLE_SCREEN_BUFFER_INFO info;
    memset(&info, 0, sizeof(info));
    if (!GetConsoleScreenBufferInfo(m_conout, &info)) {
        Trace("GetConsoleScreenBufferInfo failed");
    }
    return info.dwSize;
}

SmallRect Win32Console::windowRect()
{
    // TODO: error handling
    CONSOLE_SCREEN_BUFFER_INFO info;
    memset(&info, 0, sizeof(info));
    if (!GetConsoleScreenBufferInfo(m_conout, &info)) {
        Trace("GetConsoleScreenBufferInfo failed");
    }
    return info.srWindow;
}

void Win32Console::resizeBuffer(const Coord &size)
{
    // TODO: error handling
    if (!SetConsoleScreenBufferSize(m_conout, size)) {
        Trace("SetConsoleScreenBufferSize failed");
    }
}

void Win32Console::moveWindow(const SmallRect &rect)
{
    // TODO: error handling
    if (!SetConsoleWindowInfo(m_conout, TRUE, &rect)) {
        Trace("SetConsoleWindowInfo failed");
    }
}

void Win32Console::reposition(const Coord &newBufferSize,
                              const SmallRect &newWindowRect)
{
    // Windows has one API for resizing the screen buffer and a different one
    // for resizing the window.  It seems that either API can fail if the
    // window does not fit on the screen buffer.

    const SmallRect origWindowRect(windowRect());
    const SmallRect origBufferRect(Coord(), bufferSize());

    ASSERT(!newBufferSize.isEmpty());
    SmallRect bufferRect(Coord(), newBufferSize);
    ASSERT(bufferRect.contains(newWindowRect));

    SmallRect tempWindowRect = origWindowRect.intersected(bufferRect);
    if (tempWindowRect.width() <= 0) {
        tempWindowRect.setLeft(newBufferSize.X - 1);
        tempWindowRect.setWidth(1);
    }
    if (tempWindowRect.height() <= 0) {
        tempWindowRect.setTop(newBufferSize.Y - 1);
        tempWindowRect.setHeight(1);
    }

    // Alternatively, if we can immediately use the new window size,
    // do that instead.
    if (origBufferRect.contains(newWindowRect))
        tempWindowRect = newWindowRect;

    if (tempWindowRect != origWindowRect)
        moveWindow(tempWindowRect);
    resizeBuffer(newBufferSize);
    if (newWindowRect != tempWindowRect)
        moveWindow(newWindowRect);
}

Coord Win32Console::cursorPosition()
{
    // TODO: error handling
    CONSOLE_SCREEN_BUFFER_INFO info;
    memset(&info, 0, sizeof(info));
    if (!GetConsoleScreenBufferInfo(m_conout, &info)) {
        Trace("GetConsoleScreenBufferInfo failed");
    }
    return info.dwCursorPosition;
}

void Win32Console::setCursorPosition(const Coord &coord)
{
    // TODO: error handling
    if (!SetConsoleCursorPosition(m_conout, coord)) {
        Trace("SetConsoleCursorPosition failed");
    }
}

void Win32Console::writeInput(const INPUT_RECORD *ir, int count)
{
    // TODO: error handling
    DWORD dummy = 0;
    if (!WriteConsoleInput(m_conin, ir, count, &dummy)) {
        Trace("WriteConsoleInput failed");
    }
}

void Win32Console::read(const SmallRect &rect, CHAR_INFO *data)
{
    // TODO: error handling
    SmallRect tmp(rect);
    if (!ReadConsoleOutput(m_conout, data, rect.size(), Coord(), &tmp)) {
        Trace("ReadConsoleOutput failed [x:%d,y:%d,w:%d,h:%d]",
              rect.Left, rect.Top, rect.width(), rect.height());
    }
}

void Win32Console::write(const SmallRect &rect, const CHAR_INFO *data)
{
    // TODO: error handling
    SmallRect tmp(rect);
    if (!WriteConsoleOutput(m_conout, data, rect.size(), Coord(), &tmp)) {
        Trace("WriteConsoleOutput failed");
    }
}
