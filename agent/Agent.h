#ifndef AGENT_H
#define AGENT_H

#include <windows.h>
#include "EventLoop.h"
#include "DsrSender.h"

class Win32Console;
class ConsoleInput;
class Terminal;
class ReadBuffer;
class NamedPipe;

const int BUFFER_LINE_COUNT = 3000; // TODO: Use something like 9000.
const int MAX_CONSOLE_WIDTH = 500;

class Agent : public EventLoop, public DsrSender
{
public:
    Agent(LPCWSTR controlPipeName,
          LPCWSTR dataPipeName,
          int initialCols,
          int initialRows);
    virtual ~Agent();
    void sendDsr();

private:
    NamedPipe *makeSocket(LPCWSTR pipeName);
    void resetConsoleTracking(bool sendClear = true);

private:
    void pollControlSocket();
    void handlePacket(ReadBuffer &packet);
    int handleStartProcessPacket(ReadBuffer &packet);
    int handleSetSizePacket(ReadBuffer &packet);
    void pollDataSocket();

protected:
    virtual void onPollTimeout();
    virtual void onPipeIo(NamedPipe *namedPipe);

private:
    void markEntireWindowDirty();
    void scanForDirtyLines();
    void resizeWindow(int cols, int rows);
    void scrapeOutput();
    void freezeConsole();
    void unfreezeConsole();
    void syncMarkerText(CHAR_INFO *output);
    int findSyncMarker();
    void createSyncMarker(int row);

private:
    Win32Console *m_console;
    NamedPipe *m_controlSocket;
    NamedPipe *m_dataSocket;
    bool m_closingDataSocket;
    Terminal *m_terminal;
    ConsoleInput *m_consoleInput;
    HANDLE m_childProcess;
    int m_childExitCode;

    int m_syncRow;
    int m_syncCounter;

    int m_scrapedLineCount;
    int m_scrolledCount;
    int m_maxBufferedLine;
    CHAR_INFO (*m_bufferData)[MAX_CONSOLE_WIDTH];
    int m_dirtyWindowTop;
    int m_dirtyLineCount;
};

#endif // AGENT_H
