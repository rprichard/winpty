#define _WIN32_WINNT 0x0501
#include "Win32Console.h"
#include <QSize>
#include <QRect>
#include <windows.h>

static inline SMALL_RECT smallRectFromQRect(const QRect &rect)
{
    SMALL_RECT smallRect = { rect.left(),
                             rect.top(),
                             rect.left() + rect.width() - 1,
                             rect.top() + rect.height() - 1 };
    return smallRect;

}

Win32Console::Win32Console(QObject *parent) :
    QObject(parent)
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
    Q_ASSERT(m_conin != NULL);
    Q_ASSERT(m_conout != NULL);
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

QSize Win32Console::bufferSize()
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(m_conout, &info);
    // TODO: error handling
    return QSize(info.dwSize.X, info.dwSize.Y);
}

QRect Win32Console::windowRect()
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(m_conout, &info);
    // TODO: error handling
    return QRect(info.srWindow.Left,
                 info.srWindow.Top,
                 info.srWindow.Right - info.srWindow.Left + 1,
                 info.srWindow.Bottom - info.srWindow.Top + 1);
}

void Win32Console::resizeBuffer(const QSize &size)
{
    COORD bufferSize = { size.width(), size.height() };
    SetConsoleScreenBufferSize(m_conout, bufferSize);
    // TODO: error handling
}

void Win32Console::moveWindow(const QRect &rect)
{
    SMALL_RECT windowRect = smallRectFromQRect(rect);
    SetConsoleWindowInfo(m_conout, TRUE, &windowRect);
    // TODO: error handling
}

void Win32Console::reposition(const QSize &newBufferSize, const QRect &newWindowRect)
{
    // Windows has one API for resizing the screen buffer and a different one
    // for resizing the window.  It seems that either API can fail if the
    // window does not fit on the screen buffer.

    const QRect origWindowRect(windowRect());
    const QRect origBufferRect(QPoint(), bufferSize());

    Q_ASSERT(!newBufferSize.isEmpty());
    QRect bufferRect(QPoint(), newBufferSize);
    Q_ASSERT(bufferRect.contains(newWindowRect));

    QRect tempWindowRect = origWindowRect.intersected(bufferRect);
    if (tempWindowRect.width() <= 0) {
        tempWindowRect.setLeft(newBufferSize.width() - 1);
        tempWindowRect.setWidth(1);
    }
    if (tempWindowRect.height() <= 0) {
        tempWindowRect.setTop(newBufferSize.height() - 1);
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

QPoint Win32Console::cursorPosition()
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(m_conout, &info);
    // TODO: error handling
    return QPoint(info.dwCursorPosition.X, info.dwCursorPosition.Y);
}

void Win32Console::setCursorPosition(const QPoint &point)
{
    COORD coord = { point.x(), point.y() };
    SetConsoleCursorPosition(m_conout, coord);
    // TODO: error handling
}

void Win32Console::writeInput(const INPUT_RECORD *ir, int count)
{
    DWORD dummy = 0;
    WriteConsoleInput(m_conin, ir, count, &dummy);
    // TODO: error handling
}

void Win32Console::read(const QRect &rect, CHAR_INFO *data)
{
    COORD bufferSize = { rect.width(), rect.height() };
    COORD zeroCoord = { 0, 0 };
    SMALL_RECT smallRect = smallRectFromQRect(rect);
    ReadConsoleOutput(m_conout, data, bufferSize, zeroCoord, &smallRect);
    // TODO: error handling
}

void Win32Console::write(const QRect &rect, const CHAR_INFO *data)
{
    COORD bufferSize = { rect.width(), rect.height() };
    COORD zeroCoord = { 0, 0 };
    SMALL_RECT smallRect = smallRectFromQRect(rect);
    WriteConsoleOutput(m_conout, data, bufferSize, zeroCoord, &smallRect);
    // TODO: error handling
}
