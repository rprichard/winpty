#ifndef WIN32CONSOLE_H
#define WIN32CONSOLE_H

#include <QObject>
#include <QSize>
#include <QRect>
#include <QPoint>
#include <windows.h>

class Win32Console : public QObject
{
    Q_OBJECT
public:
    explicit Win32Console(QObject *parent = 0);
    virtual ~Win32Console();

    HANDLE conin();
    HANDLE conout();
    HWND hwnd();
    void postCloseMessage();

    // Buffer and window sizes.
    QSize bufferSize();
    QRect windowRect();
    void resizeBuffer(const QSize &size);
    void moveWindow(const QRect &rect);
    void reposition(const QSize &bufferSize, const QRect &windowRect);

    // Cursor.
    QPoint cursorPosition();
    void setCursorPosition(const QPoint &point);

    // Input stream.
    void writeInput(const INPUT_RECORD *ir, int count=1);

    // Screen content.
    void read(const QRect &rect, CHAR_INFO *data);
    void write(const QRect &rect, const CHAR_INFO *data);

signals:

public slots:

private:
    HANDLE m_conin;
    HANDLE m_conout;
};

#endif // WIN32CONSOLE_H
