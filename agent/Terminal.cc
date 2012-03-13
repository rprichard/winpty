#include "Terminal.h"
#include <QIODevice>
#include <windows.h>
#include <stdio.h>

#define CSI "\x1b["

#ifndef COMMON_LVB_REVERSE_VIDEO
#define COMMON_LVB_REVERSE_VIDEO 0x4000
#endif

const int COLOR_ATTRIBUTE_MASK =
        FOREGROUND_BLUE |
        FOREGROUND_GREEN |
        FOREGROUND_RED |
        FOREGROUND_INTENSITY |
        BACKGROUND_BLUE |
        BACKGROUND_GREEN |
        BACKGROUND_RED |
        BACKGROUND_INTENSITY |
        COMMON_LVB_REVERSE_VIDEO;

const int TERMINAL_RED   = 1;
const int TERMINAL_GREEN = 2;
const int TERMINAL_BLUE  = 4;

const int TERMINAL_FOREGROUND = 30;
const int TERMINAL_BACKGROUND = 40;

Terminal::Terminal(QIODevice *output, QObject *parent) :
    QObject(parent),
    m_output(output),
    m_remoteLine(0),
    m_cursorHidden(false),
    m_remoteColor(-1)
{
}

void Terminal::reset(bool sendClearFirst, int newLine)
{
    if (sendClearFirst)
        m_output->write(CSI"1;1H"CSI"2J");
    m_remoteLine = newLine;
    m_cursorHidden = false;
    m_cursorPos = Coord(0, newLine);
    m_remoteColor = -1;
}

void Terminal::sendLine(int line, CHAR_INFO *lineData, int width)
{
    hideTerminalCursor();
    moveTerminalToLine(line);

    // Erase in Line -- erase entire line.
    m_output->write(CSI"2K");

    QByteArray termLine;
    termLine.reserve(width + 32);

    int length = 0;
    for (int i = 0; i < width; ++i) {
        int color = lineData[i].Attributes & COLOR_ATTRIBUTE_MASK;
        if (color != m_remoteColor) {
            int fore = 0;
            int back = 0;
            if (color & FOREGROUND_RED)   fore |= TERMINAL_RED;
            if (color & FOREGROUND_GREEN) fore |= TERMINAL_GREEN;
            if (color & FOREGROUND_BLUE)  fore |= TERMINAL_BLUE;
            if (color & BACKGROUND_RED)   back |= TERMINAL_RED;
            if (color & BACKGROUND_GREEN) back |= TERMINAL_GREEN;
            if (color & BACKGROUND_BLUE)  back |= TERMINAL_BLUE;
            char buffer[128];
            sprintf(buffer, CSI"0;%d;%d",
                    TERMINAL_FOREGROUND + fore,
                    TERMINAL_BACKGROUND + back);
            if (color & FOREGROUND_INTENSITY)
                strcat(buffer, ";1");
            if (color & COMMON_LVB_REVERSE_VIDEO)
                strcat(buffer, ";7");
            strcat(buffer, "m");
            termLine.append(buffer);
            length = termLine.size();
            m_remoteColor = color;
        }
        // TODO: Unicode
        char ch = lineData[i].Char.AsciiChar;
        if (ch == ' ') {
            termLine.append(' ');
        } else {
            termLine.append(isprint(ch) ? ch : '?');
            length = termLine.size();
        }
    }

    termLine.truncate(length);
    m_output->write(termLine);
}

void Terminal::finishOutput(const Coord &newCursorPos)
{
    if (newCursorPos != m_cursorPos)
        hideTerminalCursor();
    if (m_cursorHidden) {
        moveTerminalToLine(newCursorPos.Y);
        char buffer[32];
        sprintf(buffer, CSI"%dG"CSI"?25h", newCursorPos.X + 1);
        m_output->write(buffer);
        m_cursorHidden = false;
    }
    m_cursorPos = newCursorPos;
}

void Terminal::hideTerminalCursor()
{
    if (m_cursorHidden)
        return;
    m_output->write(CSI"?25l");
    m_cursorHidden = true;
}

void Terminal::moveTerminalToLine(int line)
{
    // Do not use CPL or CNL.  Konsole 2.5.4 does not support Cursor Previous
    // Line (CPL) -- there are "Undecodable sequence" errors.  gnome-terminal
    // 2.32.0 does handle it.  Cursor Next Line (CNL) does nothing if the
    // cursor is on the last line already.

    if (line < m_remoteLine) {
        // Cursor Horizontal Absolute (CHA) -- move to column 1.
        // CUrsor Up (CUU)
        char buffer[32];
        sprintf(buffer, CSI"1G"CSI"%dA", m_remoteLine - line);
        m_output->write(buffer);
        m_remoteLine = line;
    } else if (line > m_remoteLine) {
        while (line > m_remoteLine) {
            m_output->write("\r\n");
            m_remoteLine++;
        }
    } else {
        // Cursor Horizontal Absolute -- move to column 1.
        m_output->write(CSI"1G");
    }
}
