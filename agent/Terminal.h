#ifndef TERMINAL_H
#define TERMINAL_H

#include <windows.h>
#include "Coord.h"
#include <utility>

class NamedPipe;

class Terminal
{
public:
    explicit Terminal(NamedPipe *output);
    void reset(bool sendClearFirst, int newLine);
    void sendLine(int line, CHAR_INFO *lineData, int width);
    void finishOutput(const std::pair<int, int> &newCursorPos);

private:
    void hideTerminalCursor();
    void moveTerminalToLine(int line);

private:
    NamedPipe *m_output;
    int m_remoteLine;
    bool m_cursorHidden;
    std::pair<int, int> m_cursorPos;
    int m_remoteColor;
};

#endif // TERMINAL_H
