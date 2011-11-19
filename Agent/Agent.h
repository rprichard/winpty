#ifndef AGENT_H
#define AGENT_H

#include <QObject>
#include <windows.h>

class Win32Console;
class QLocalSocket;
class QTimer;

const int BUFFER_LINE_COUNT = 3000; // TODO: Use something like 9000.
const int MAX_CONSOLE_WIDTH = 500;

class Agent : public QObject
{
    Q_OBJECT
public:
    explicit Agent(const QString &socketServer,
                   int initialCols, int initialRows,
                   QObject *parent = 0);
    virtual ~Agent();

signals:

private slots:
    void socketReadyRead();
    void socketDisconnected();
    void pollTimeout();

private:
    void resizeWindow(int cols, int rows);
    void scrapeOutput();
    void freezeConsole();
    void unfreezeConsole();
    void syncMarkerText(CHAR_INFO *output);
    int findSyncMarker();
    void createSyncMarker(int row);

    void moveTerminalToLine(int line);
    void sendLineToTerminal(int line, CHAR_INFO *lineData, int width);
    void hideTerminalCursor();
    void showTerminalCursor(int line, int column);

private:
    Win32Console *m_console;
    QLocalSocket *m_socket;
    QTimer *m_timer;

    int m_syncRow;
    int m_syncCounter;

    int m_scrapedLineCount;
    int m_scrolledCount;
    int m_maxBufferedLine;
    CHAR_INFO (*m_bufferData)[MAX_CONSOLE_WIDTH];

    int m_remoteLine;
};

#endif // AGENT_H
