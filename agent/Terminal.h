#ifndef TERMINAL_H
#define TERMINAL_H

#include <QObject>
#include <QPoint>
#include <windows.h>
#include "Coord.h"

class QIODevice;

class Terminal : public QObject
{
    Q_OBJECT
public:
    explicit Terminal(QIODevice *output, QObject *parent = 0);
    void reset(bool sendClearFirst, int newLine);
    void sendLine(int line, CHAR_INFO *lineData, int width);
    void finishOutput(const Coord &newCursorPos);

private:
    void hideTerminalCursor();
    void moveTerminalToLine(int line);

private:
    QIODevice *m_output;
    int m_remoteLine;
    bool m_cursorHidden;
    Coord m_cursorPos;
    int m_remoteColor;
};

#endif // TERMINAL_H
