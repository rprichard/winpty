#ifndef LARGE_CONSOLE_READ_H
#define LARGE_CONSOLE_READ_H

#include <windows.h>

#include <vector>

#include "SmallRect.h"
#include "../shared/WinptyAssert.h"

class Win32Console;

class LargeConsoleReadBuffer {
public:
    LargeConsoleReadBuffer();
    const SmallRect &rect() const { return m_rect; }
    const CHAR_INFO *lineData(int line) const {
        ASSERT(line >= m_rect.Top && line <= m_rect.Bottom);
        return &m_data[(line - m_rect.Top) * m_rectWidth];
    }

private:
    CHAR_INFO *lineDataMut(int line) {
        ASSERT(line >= m_rect.Top && line <= m_rect.Bottom);
        return &m_data[(line - m_rect.Top) * m_rectWidth];
    }

    SmallRect m_rect;
    int m_rectWidth;
    std::vector<CHAR_INFO> m_data;

    friend void largeConsoleRead(LargeConsoleReadBuffer &out,
                                 Win32Console &console,
                                 const SmallRect &readArea);
};

#endif // LARGE_CONSOLE_READ_H
