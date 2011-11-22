#include "Terminal.h"
#include <QIODevice>
#include <windows.h>
#include <stdio.h>

#define CSI "\x1b["

Terminal::Terminal(QIODevice *output, QObject *parent) :
    QObject(parent),
    m_output(output),
    m_remoteLine(0),
    m_cursorHidden(false)
{
}

void Terminal::reset(bool sendClearFirst, int newLine)
{
    if (sendClearFirst)
        m_output->write(CSI"1;1H"CSI"2J");
    m_remoteLine = newLine;
    m_cursorHidden = false;
    m_cursorPos = QPoint(0, newLine);
}

void Terminal::sendLine(int line, CHAR_INFO *lineData, int width)
{
    hideTerminalCursor();
    moveTerminalToLine(line);

    // Erase in Line -- erase entire line.
    m_output->write(CSI"2K");

    char buffer[512];
    int length = 0;
    for (int i = 0; i < width; ++i) {
        buffer[i] = lineData[i].Char.AsciiChar;
        if (buffer[i] != ' ')
            length = i + 1;
    }

    m_output->write(buffer, length);
}

void Terminal::finishOutput(QPoint newCursorPos)
{
    if (newCursorPos != m_cursorPos)
        hideTerminalCursor();
    if (m_cursorHidden) {
        moveTerminalToLine(newCursorPos.y());
        char buffer[32];
        sprintf(buffer, CSI"%dG"CSI"?25h", newCursorPos.x() + 1);
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
