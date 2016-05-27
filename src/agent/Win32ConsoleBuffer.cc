// Copyright (c) 2011-2016 Ryan Prichard
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

#include "Win32ConsoleBuffer.h"

#include <windows.h>

#include "../shared/DebugClient.h"
#include "../shared/WinptyAssert.h"

std::unique_ptr<Win32ConsoleBuffer> Win32ConsoleBuffer::openStdout() {
    return std::unique_ptr<Win32ConsoleBuffer>(
        new Win32ConsoleBuffer(GetStdHandle(STD_OUTPUT_HANDLE), false));
}

std::unique_ptr<Win32ConsoleBuffer> Win32ConsoleBuffer::openConout() {
    const HANDLE conout = CreateFileW(L"CONOUT$",
                                      GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL, OPEN_EXISTING, 0, NULL);
    ASSERT(conout != INVALID_HANDLE_VALUE);
    return std::unique_ptr<Win32ConsoleBuffer>(
        new Win32ConsoleBuffer(conout, true));
}

std::unique_ptr<Win32ConsoleBuffer> Win32ConsoleBuffer::createErrorBuffer() {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    const HANDLE conout =
        CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  &sa,
                                  CONSOLE_TEXTMODE_BUFFER,
                                  nullptr);
    ASSERT(conout != INVALID_HANDLE_VALUE);
    return std::unique_ptr<Win32ConsoleBuffer>(
        new Win32ConsoleBuffer(conout, true));
}

HANDLE Win32ConsoleBuffer::conout() {
    return m_conout;
}

void Win32ConsoleBuffer::clearLines(
        int row,
        int count,
        const ConsoleScreenBufferInfo &info) {
    // TODO: error handling
    const int width = info.bufferSize().X;
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

void Win32ConsoleBuffer::clearAllLines(const ConsoleScreenBufferInfo &info) {
    clearLines(0, info.bufferSize().Y, info);
}

ConsoleScreenBufferInfo Win32ConsoleBuffer::bufferInfo() {
    // TODO: error handling
    ConsoleScreenBufferInfo info;
    if (!GetConsoleScreenBufferInfo(m_conout, &info)) {
        trace("GetConsoleScreenBufferInfo failed");
    }
    return info;
}

Coord Win32ConsoleBuffer::bufferSize() {
    return bufferInfo().bufferSize();
}

SmallRect Win32ConsoleBuffer::windowRect() {
    return bufferInfo().windowRect();
}

void Win32ConsoleBuffer::resizeBuffer(const Coord &size) {
    // TODO: error handling
    if (!SetConsoleScreenBufferSize(m_conout, size)) {
        trace("SetConsoleScreenBufferSize failed");
    }
}

void Win32ConsoleBuffer::moveWindow(const SmallRect &rect) {
    // TODO: error handling
    if (!SetConsoleWindowInfo(m_conout, TRUE, &rect)) {
        trace("SetConsoleWindowInfo failed");
    }
}

Coord Win32ConsoleBuffer::cursorPosition() {
    return bufferInfo().dwCursorPosition;
}

void Win32ConsoleBuffer::setCursorPosition(const Coord &coord) {
    // TODO: error handling
    if (!SetConsoleCursorPosition(m_conout, coord)) {
        trace("SetConsoleCursorPosition failed");
    }
}

void Win32ConsoleBuffer::read(const SmallRect &rect, CHAR_INFO *data) {
    // TODO: error handling
    SmallRect tmp(rect);
    if (!ReadConsoleOutputW(m_conout, data, rect.size(), Coord(), &tmp)) {
        trace("ReadConsoleOutput failed [x:%d,y:%d,w:%d,h:%d]",
              rect.Left, rect.Top, rect.width(), rect.height());
    }
}

void Win32ConsoleBuffer::write(const SmallRect &rect, const CHAR_INFO *data) {
    // TODO: error handling
    SmallRect tmp(rect);
    if (!WriteConsoleOutputW(m_conout, data, rect.size(), Coord(), &tmp)) {
        trace("WriteConsoleOutput failed");
    }
}

void Win32ConsoleBuffer::setTextAttribute(WORD attributes) {
    if (!SetConsoleTextAttribute(m_conout, attributes)) {
        trace("SetConsoleTextAttribute failed");
    }
}
