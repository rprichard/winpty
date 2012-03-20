#ifndef WIN32CONSOLE_H
#define WIN32CONSOLE_H

#include <windows.h>
#include "Coord.h"
#include "SmallRect.h"

class Win32Console
{
public:
    Win32Console();
    ~Win32Console();

    HANDLE conin();
    HANDLE conout();
    HWND hwnd();
    void postCloseMessage();

    // Buffer and window sizes.
    Coord bufferSize();
    SmallRect windowRect();
    void resizeBuffer(const Coord &size);
    void moveWindow(const SmallRect &rect);
    void reposition(const Coord &bufferSize, const SmallRect &windowRect);

    // Cursor.
    Coord cursorPosition();
    void setCursorPosition(const Coord &point);

    // Input stream.
    void writeInput(const INPUT_RECORD *ir, int count=1);
    bool processedInputMode();

    // Screen content.
    void read(const SmallRect &rect, CHAR_INFO *data);
    void write(const SmallRect &rect, const CHAR_INFO *data);

private:
    HANDLE m_conin;
    HANDLE m_conout;
};

#endif // WIN32CONSOLE_H
