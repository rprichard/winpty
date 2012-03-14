#include "Win32Console.h"
#include "AgentAssert.h"
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
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(m_conout, &info);
    // TODO: error handling
    return info.dwSize;
}

SmallRect Win32Console::windowRect()
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(m_conout, &info);
    // TODO: error handling
    return info.srWindow;
}

void Win32Console::resizeBuffer(const Coord &size)
{
    SetConsoleScreenBufferSize(m_conout, size);
    // TODO: error handling
}

void Win32Console::moveWindow(const SmallRect &rect)
{
    SetConsoleWindowInfo(m_conout, TRUE, &rect);
    // TODO: error handling
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
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(m_conout, &info);
    // TODO: error handling
    return info.dwCursorPosition;
}

void Win32Console::setCursorPosition(const Coord &coord)
{
    SetConsoleCursorPosition(m_conout, coord);
    // TODO: error handling
}

void Win32Console::writeInput(const INPUT_RECORD *ir, int count)
{
    DWORD dummy = 0;
    WriteConsoleInput(m_conin, ir, count, &dummy);
    // TODO: error handling
}

void Win32Console::read(const SmallRect &rect, CHAR_INFO *data)
{
    SmallRect tmp(rect);
    ReadConsoleOutput(m_conout, data, rect.size(), Coord(), &tmp);
    // TODO: error handling
}

void Win32Console::write(const SmallRect &rect, const CHAR_INFO *data)
{
    SmallRect tmp(rect);
    WriteConsoleOutput(m_conout, data, rect.size(), Coord(), &tmp);
    // TODO: error handling
}
