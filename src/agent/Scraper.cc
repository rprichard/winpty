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

#include "Scraper.h"

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <utility>

#include "../shared/WinptyAssert.h"
#include "../shared/winpty_snprintf.h"

#include "ConsoleFont.h"
#include "Win32Console.h"
#include "Win32ConsoleBuffer.h"

namespace {

template <typename T>
T constrained(T min, T val, T max) {
    ASSERT(min <= max);
    return std::min(std::max(min, val), max);
}

} // anonymous namespace

Scraper::Scraper(
        Win32Console &console,
        Win32ConsoleBuffer &buffer,
        std::unique_ptr<Terminal> terminal,
        Coord initialSize) :
    m_console(console),
    m_terminal(std::move(terminal)),
    m_ptySize(initialSize)
{
    m_consoleBuffer = &buffer;

    resetConsoleTracking(Terminal::OmitClear, buffer.windowRect());

    m_bufferData.resize(BUFFER_LINE_COUNT);

    setSmallFont(buffer.conout());
    buffer.moveWindow(SmallRect(0, 0, 1, 1));
    buffer.resizeBuffer(Coord(initialSize.X, BUFFER_LINE_COUNT));
    buffer.moveWindow(SmallRect(0, 0, initialSize.X, initialSize.Y));
    buffer.setCursorPosition(Coord(0, 0));

    // For the sake of the color translation heuristic, set the console color
    // to LtGray-on-Black.
    buffer.setTextAttribute(7);
    buffer.clearAllLines(m_consoleBuffer->bufferInfo());

    m_consoleBuffer = nullptr;
}

Scraper::~Scraper()
{
}

void Scraper::resizeWindow(Win32ConsoleBuffer &buffer,
                           Coord newSize,
                           ConsoleScreenBufferInfo &finalInfoOut)
{
    m_consoleBuffer = &buffer;
    m_ptySize = newSize;
    syncConsoleContentAndSize(true, finalInfoOut);
    m_consoleBuffer = nullptr;
}

void Scraper::scrapeBuffer(Win32ConsoleBuffer &buffer,
                           ConsoleScreenBufferInfo &finalInfoOut)
{
    m_consoleBuffer = &buffer;
    syncConsoleContentAndSize(false, finalInfoOut);
    m_consoleBuffer = nullptr;
}

void Scraper::resetConsoleTracking(
    Terminal::SendClearFlag sendClear, const SmallRect &windowRect)
{
    for (ConsoleLine &line : m_bufferData) {
        line.reset();
    }
    m_syncRow = -1;
    m_scrapedLineCount = windowRect.top();
    m_scrolledCount = 0;
    m_maxBufferedLine = -1;
    m_dirtyWindowTop = -1;
    m_dirtyLineCount = 0;
    m_terminal->reset(sendClear, m_scrapedLineCount);
}

// Detect window movement.  If the window moves down (presumably as a
// result of scrolling), then assume that all screen buffer lines down to
// the bottom of the window are dirty.
void Scraper::markEntireWindowDirty(const SmallRect &windowRect)
{
    m_dirtyLineCount = std::max(m_dirtyLineCount,
                                windowRect.top() + windowRect.height());
}

// Scan the screen buffer and advance the dirty line count when we find
// non-empty lines.
void Scraper::scanForDirtyLines(const SmallRect &windowRect)
{
    const int w = m_readBuffer.rect().width();
    ASSERT(m_dirtyLineCount >= 1);
    const CHAR_INFO *const prevLine =
        m_readBuffer.lineData(m_dirtyLineCount - 1);
    WORD prevLineAttr = prevLine[w - 1].Attributes;
    const int stopLine = windowRect.top() + windowRect.height();

    for (int line = m_dirtyLineCount; line < stopLine; ++line) {
        const CHAR_INFO *lineData = m_readBuffer.lineData(line);
        for (int col = 0; col < w; ++col) {
            const WORD colAttr = lineData[col].Attributes;
            if (lineData[col].Char.UnicodeChar != L' ' ||
                    colAttr != prevLineAttr) {
                m_dirtyLineCount = line + 1;
                break;
            }
        }
        prevLineAttr = lineData[w - 1].Attributes;
    }
}

// Clear lines in the line buffer.  The `firstRow` parameter is in
// screen-buffer coordinates.
void Scraper::clearBufferLines(
        const int firstRow,
        const int count,
        const WORD attributes)
{
    ASSERT(!m_directMode);
    for (int row = firstRow; row < firstRow + count; ++row) {
        const int64_t bufLine = row + m_scrolledCount;
        m_maxBufferedLine = std::max(m_maxBufferedLine, bufLine);
        m_bufferData[bufLine % BUFFER_LINE_COUNT].blank(attributes);
    }
}

void Scraper::resizeImpl(const ConsoleScreenBufferInfo &origInfo)
{
    ASSERT(m_console.frozen());
    const int cols = m_ptySize.X;
    const int rows = m_ptySize.Y;

    {
        //
        // To accommodate Windows 10, erase all lines up to the top of the
        // visible window.  It's hard to tell whether this is strictly
        // necessary.  It ensures that the sync marker won't move downward,
        // and it ensures that we won't repeat lines that have already scrolled
        // up into the scrollback.
        //
        // It *is* possible for these blank lines to reappear in the visible
        // window (e.g. if the window is made taller), but because we blanked
        // the lines in the line buffer, we still don't output them again.
        //
        const Coord origBufferSize = origInfo.bufferSize();
        const SmallRect origWindowRect = origInfo.windowRect();

        if (!m_directMode) {
            m_consoleBuffer->clearLines(0, origWindowRect.Top, origInfo);
            clearBufferLines(0, origWindowRect.Top, origInfo.wAttributes);
            if (m_syncRow != -1) {
                createSyncMarker(m_syncRow);
            }
        }

        const Coord finalBufferSize(
            cols,
            // If there was previously no scrollback (e.g. a full-screen app
            // in direct mode) and we're reducing the window height, then
            // reduce the console buffer's height too.
            (origWindowRect.height() == origBufferSize.Y)
                ? rows
                : std::max<int>(rows, origBufferSize.Y));
        const bool cursorWasInWindow =
            origInfo.cursorPosition().Y >= origWindowRect.Top &&
            origInfo.cursorPosition().Y <= origWindowRect.Bottom;

        // Step 1: move the window.
        const int tmpWindowWidth = std::min(origBufferSize.X, finalBufferSize.X);
        const int tmpWindowHeight = std::min<int>(origBufferSize.Y, rows);
        SmallRect tmpWindowRect(
            0,
            std::min<int>(origBufferSize.Y - tmpWindowHeight,
                          origWindowRect.Top),
            tmpWindowWidth,
            tmpWindowHeight);
        if (cursorWasInWindow) {
            tmpWindowRect = tmpWindowRect.ensureLineIncluded(
                origInfo.cursorPosition().Y);
        }
        m_consoleBuffer->moveWindow(tmpWindowRect);

        // Step 2: resize the buffer.
        m_console.setFrozen(false);
        m_consoleBuffer->resizeBuffer(finalBufferSize);
    }

    // Step 3: expand the window to its full size.
    {
        m_console.setFrozen(true);
        const ConsoleScreenBufferInfo info = m_consoleBuffer->bufferInfo();
        const bool cursorWasInWindow =
            info.cursorPosition().Y >= info.windowRect().Top &&
            info.cursorPosition().Y <= info.windowRect().Bottom;

        SmallRect finalWindowRect(
            0,
            std::min<int>(info.bufferSize().Y - rows,
                          info.windowRect().Top),
            cols,
            rows);

        //
        // Once a line in the screen buffer is "dirty", it should stay visible
        // in the console window, so that we continue to update its content in
        // the terminal.  This code is particularly (only?) necessary on
        // Windows 10, where making the buffer wider can rewrap lines and move
        // the console window upward.
        //
        if (!m_directMode && m_dirtyLineCount > finalWindowRect.Bottom + 1) {
            // In theory, we avoid ensureLineIncluded, because, a massive
            // amount of output could have occurred while the console was
            // unfrozen, so that the *top* of the window is now below the
            // dirtiest tracked line.
            finalWindowRect = SmallRect(
                0, m_dirtyLineCount - rows,
                cols, rows);
        }

        // Highest priority constraint: ensure that the cursor remains visible.
        if (cursorWasInWindow) {
            finalWindowRect = finalWindowRect.ensureLineIncluded(
                info.cursorPosition().Y);
        }

        m_consoleBuffer->moveWindow(finalWindowRect);
        m_dirtyWindowTop = finalWindowRect.Top;
    }

    ASSERT(m_console.frozen());
}

void Scraper::syncConsoleContentAndSize(
    bool forceResize,
    ConsoleScreenBufferInfo &finalInfoOut)
{
    Win32Console::FreezeGuard guard(m_console, true);

    const ConsoleScreenBufferInfo info = m_consoleBuffer->bufferInfo();
    BOOL cursorVisible = true;
    CONSOLE_CURSOR_INFO cursorInfo = {};
    if (!GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo)) {
        trace("GetConsoleCursorInfo failed");
    } else {
        cursorVisible = cursorInfo.bVisible;
    }

    // If an app resizes the buffer height, then we enter "direct mode", where
    // we stop trying to track incremental console changes.
    const bool newDirectMode = (info.bufferSize().Y != BUFFER_LINE_COUNT);
    if (newDirectMode != m_directMode) {
        trace("Entering %s mode", newDirectMode ? "direct" : "scrolling");
        resetConsoleTracking(Terminal::SendClear, info.windowRect());
        m_directMode = newDirectMode;

        // When we switch from direct->scrolling mode, make sure the console is
        // the right size.
        if (!m_directMode) {
            forceResize = true;
        }
    }

    if (m_directMode) {
        directScrapeOutput(info, cursorVisible);
    } else {
        scrollingScrapeOutput(info, cursorVisible);
    }

    if (forceResize) {
        resizeImpl(info);
        finalInfoOut = m_consoleBuffer->bufferInfo();
    } else {
        finalInfoOut = info;
    }
}

void Scraper::directScrapeOutput(const ConsoleScreenBufferInfo &info,
                                 bool cursorVisible)
{
    const SmallRect windowRect = info.windowRect();

    const SmallRect scrapeRect(
        windowRect.left(), windowRect.top(),
        std::min<SHORT>(std::min(windowRect.width(), m_ptySize.X),
                        MAX_CONSOLE_WIDTH),
        std::min<SHORT>(std::min(windowRect.height(), m_ptySize.Y),
                        BUFFER_LINE_COUNT));
    const int w = scrapeRect.width();
    const int h = scrapeRect.height();

    const Coord cursor = info.cursorPosition();
    const int cursorColumn = !cursorVisible ? -1 :
        constrained(0, cursor.X - scrapeRect.Left, w - 1);
    const int cursorLine = !cursorVisible ? -1 :
        constrained(0, cursor.Y - scrapeRect.Top, h - 1);
    if (!cursorVisible) {
        m_terminal->hideTerminalCursor();
    }

    largeConsoleRead(m_readBuffer, *m_consoleBuffer, scrapeRect);

    bool sawModifiedLine = false;
    for (int line = 0; line < h; ++line) {
        const CHAR_INFO *curLine =
            m_readBuffer.lineData(scrapeRect.top() + line);
        ConsoleLine &bufLine = m_bufferData[line];
        if (sawModifiedLine) {
            bufLine.setLine(curLine, w);
        } else {
            sawModifiedLine = bufLine.detectChangeAndSetLine(curLine, w);
        }
        if (sawModifiedLine) {
            const int lineCursorColumn =
                line == cursorLine ? cursorColumn : -1;
            m_terminal->sendLine(line, curLine, w, lineCursorColumn);
        }
    }

    if (cursorVisible) {
        m_terminal->showTerminalCursor(cursorColumn, cursorLine);
    }
}

void Scraper::scrollingScrapeOutput(const ConsoleScreenBufferInfo &info,
                                    bool cursorVisible)
{
    const Coord cursor = info.cursorPosition();
    const SmallRect windowRect = info.windowRect();

    if (m_syncRow != -1) {
        // If a synchronizing marker was placed into the history, look for it
        // and adjust the scroll count.
        int markerRow = findSyncMarker();
        if (markerRow == -1) {
            // Something has happened.  Reset the terminal.
            trace("Sync marker has disappeared -- resetting the terminal"
                  " (m_syncCounter=%u)",
                  m_syncCounter);
            resetConsoleTracking(Terminal::SendClear, windowRect);
        } else if (markerRow != m_syncRow) {
            ASSERT(markerRow < m_syncRow);
            m_scrolledCount += (m_syncRow - markerRow);
            m_syncRow = markerRow;
            // If the buffer has scrolled, then the entire window is dirty.
            markEntireWindowDirty(windowRect);
        }
    }

    // Update the dirty line count:
    //  - If the window has moved, the entire window is dirty.
    //  - Everything up to the cursor is dirty.
    //  - All lines above the window are dirty.
    //  - Any non-blank lines are dirty.
    if (m_dirtyWindowTop != -1) {
        if (windowRect.top() > m_dirtyWindowTop) {
            // The window has moved down, presumably as a result of scrolling.
            markEntireWindowDirty(windowRect);
        } else if (windowRect.top() < m_dirtyWindowTop) {
            // The window has moved upward.  This is generally not expected to
            // happen, but the CMD/PowerShell CLS command will move the window
            // to the top as part of clearing everything else in the console.
            trace("Window moved upward -- resetting the terminal"
                  " (m_syncCounter=%u)",
                  m_syncCounter);
            resetConsoleTracking(Terminal::SendClear, windowRect);
        }
    }
    m_dirtyWindowTop = windowRect.top();
    m_dirtyLineCount = std::max(m_dirtyLineCount, cursor.Y + 1);
    m_dirtyLineCount = std::max(m_dirtyLineCount, (int)windowRect.top());

    // There will be at least one dirty line, because there is a cursor.
    ASSERT(m_dirtyLineCount >= 1);

    // The first line to scrape, in virtual line coordinates.
    const int64_t firstVirtLine = std::min(m_scrapedLineCount,
                                           windowRect.top() + m_scrolledCount);

    // Read all the data we will need from the console.  Start reading with the
    // first line to scrape, but adjust the the read area upward to account for
    // scanForDirtyLines' need to read the previous attribute.  Read to the
    // bottom of the window.  (It's not clear to me whether the
    // m_dirtyLineCount adjustment here is strictly necessary.  It isn't
    // necessary so long as the cursor is inside the current window.)
    const int firstReadLine = std::min<int>(firstVirtLine - m_scrolledCount,
                                            m_dirtyLineCount - 1);
    const int stopReadLine = std::max(windowRect.top() + windowRect.height(),
                                      m_dirtyLineCount);
    ASSERT(firstReadLine >= 0 && stopReadLine > firstReadLine);
    largeConsoleRead(m_readBuffer,
                     *m_consoleBuffer,
                     SmallRect(0, firstReadLine,
                               std::min<SHORT>(info.bufferSize().X,
                                               MAX_CONSOLE_WIDTH),
                               stopReadLine - firstReadLine));

    scanForDirtyLines(windowRect);

    // Note that it's possible for all the lines on the current window to
    // be non-dirty.

    // The line to stop scraping at, in virtual line coordinates.
    const int64_t stopVirtLine =
        std::min(m_dirtyLineCount, windowRect.top() + windowRect.height()) +
            m_scrolledCount;

    const int64_t cursorLine = !cursorVisible ? -1 : cursor.Y + m_scrolledCount;
    const int cursorColumn = !cursorVisible ? -1 : cursor.X;
    if (!cursorVisible) {
        m_terminal->hideTerminalCursor();
    }

    bool sawModifiedLine = false;

    const int w = m_readBuffer.rect().width();
    for (int64_t line = firstVirtLine; line < stopVirtLine; ++line) {
        const CHAR_INFO *curLine =
            m_readBuffer.lineData(line - m_scrolledCount);
        ConsoleLine &bufLine = m_bufferData[line % BUFFER_LINE_COUNT];
        if (line > m_maxBufferedLine) {
            m_maxBufferedLine = line;
            sawModifiedLine = true;
        }
        if (sawModifiedLine) {
            bufLine.setLine(curLine, w);
        } else {
            sawModifiedLine = bufLine.detectChangeAndSetLine(curLine, w);
        }
        if (sawModifiedLine) {
            const int lineCursorColumn =
                line == cursorLine ? cursorColumn : -1;
            m_terminal->sendLine(line, curLine, w, lineCursorColumn);
        }
    }

    m_scrapedLineCount = windowRect.top() + m_scrolledCount;

    // Creating a new sync row requires clearing part of the console buffer, so
    // avoid doing it if there's already a sync row that's good enough.
    // TODO: replace hard-coded constants
    const int newSyncRow = static_cast<int>(windowRect.top()) - 200;
    if (newSyncRow >= 1 && newSyncRow >= m_syncRow + 200) {
        createSyncMarker(newSyncRow);
    }

    if (cursorVisible) {
        m_terminal->showTerminalCursor(cursorColumn, cursorLine);
    }
}

void Scraper::syncMarkerText(CHAR_INFO (&output)[SYNC_MARKER_LEN])
{
    // XXX: The marker text generated here could easily collide with ordinary
    // console output.  Does it make sense to try to avoid the collision?
    char str[SYNC_MARKER_LEN + 1];
    winpty_snprintf(str, "S*Y*N*C*%08x", m_syncCounter);
    for (int i = 0; i < SYNC_MARKER_LEN; ++i) {
        output[i].Char.UnicodeChar = str[i];
        output[i].Attributes = 7;
    }
}

int Scraper::findSyncMarker()
{
    ASSERT(m_syncRow >= 0);
    CHAR_INFO marker[SYNC_MARKER_LEN];
    CHAR_INFO column[BUFFER_LINE_COUNT];
    syncMarkerText(marker);
    SmallRect rect(0, 0, 1, m_syncRow + SYNC_MARKER_LEN);
    m_consoleBuffer->read(rect, column);
    int i;
    for (i = m_syncRow; i >= 0; --i) {
        int j;
        for (j = 0; j < SYNC_MARKER_LEN; ++j) {
            if (column[i + j].Char.UnicodeChar != marker[j].Char.UnicodeChar)
                break;
        }
        if (j == SYNC_MARKER_LEN)
            return i;
    }
    return -1;
}

void Scraper::createSyncMarker(int row)
{
    ASSERT(row >= 1);

    // Clear the lines around the marker to ensure that Windows 10's rewrapping
    // does not affect the marker.
    m_consoleBuffer->clearLines(row - 1, SYNC_MARKER_LEN + 1,
                                m_consoleBuffer->bufferInfo());

    // Write a new marker.
    m_syncCounter++;
    CHAR_INFO marker[SYNC_MARKER_LEN];
    syncMarkerText(marker);
    m_syncRow = row;
    SmallRect markerRect(0, m_syncRow, 1, SYNC_MARKER_LEN);
    m_consoleBuffer->write(markerRect, marker);
}
