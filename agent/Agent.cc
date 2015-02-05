// Copyright (c) 2011-2012 Ryan Prichard
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

#include "Agent.h"
#include "ConsoleInput.h"
#include "Terminal.h"
#include "NamedPipe.h"
#include "AgentAssert.h"
#include "../shared/DebugClient.h"
#include "../shared/AgentMsg.h"
#include "../shared/Buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include <vector>
#include <string>
#include <utility>

const int SC_CONSOLE_MARK = 0xFFF2;
const int SC_CONSOLE_SELECT_ALL = 0xFFF5;
const int SYNC_MARKER_LEN = 16;

static BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType)
{
    if (dwCtrlType == CTRL_C_EVENT) {
        // Do nothing and claim to have handled the event.
        return TRUE;
    }
    return FALSE;
}

Agent::Agent(bool consoleMode,
             LPCWSTR controlPipeName,
             LPCWSTR dataPipeName,
             LPCWSTR errDataPipeName,
             int initialCols,
             int initialRows) :
    m_closingDataSocket(false),
    m_consoleMode(consoleMode),
    m_childProcess(NULL),
    m_childExitCode(-1)
{
    trace("Agent starting...");

    m_conout.type = CONOUT;
    m_conout.syncCounter = 0;
    m_conout.bufferData = new CHAR_INFO[BUFFER_LINE_COUNT][MAX_CONSOLE_WIDTH];

    m_console = new Win32Console(consoleMode);
    initConsole(m_conout, initialCols, initialRows);

    if (consoleMode) {
        m_conerr.type = CONERR;
        m_conerr.syncCounter = 0;
        m_conerr.bufferData = new CHAR_INFO[BUFFER_LINE_COUNT][MAX_CONSOLE_WIDTH];
        initConsole(m_conerr, initialCols, initialRows);
    }

    m_dataSocket = makeSocket(dataPipeName);
    m_controlSocket = makeSocket(controlPipeName);

    m_conout.terminal = new Terminal(m_dataSocket);
    m_conout.terminal->setConsoleMode(consoleMode);

    if (consoleMode) {
        m_errDataSocket = makeSocket(errDataPipeName);
        m_conerr.terminal = new Terminal(m_errDataSocket);
        m_conerr.terminal->setConsoleMode(true);
    }

    m_consoleInput = new ConsoleInput(m_console, this);

    resetConsoleTracking(m_conout, false);
    if (consoleMode) resetConsoleTracking(m_conerr, false);

    // Setup Ctrl-C handling.  First restore default handling of Ctrl-C.  This
    // attribute is inherited by child processes.  Then register a custom
    // Ctrl-C handler that does nothing.  The handler will be called when the
    // agent calls GenerateConsoleCtrlEvent.
    SetConsoleCtrlHandler(NULL, FALSE);
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);

    setPollInterval(25);
}

void Agent::initConsole(con_status_s con, int initialCols, int initialRows) {
    m_console->switchConsoleBuffer(con.type);
    m_console->setSmallFont();
    m_console->reposition(
                Coord(initialCols, BUFFER_LINE_COUNT),
                SmallRect(0, 0, initialCols, initialRows));
    m_console->setCursorPosition(Coord(0, 0));
}

Agent::~Agent()
{
    trace("Agent exiting...");
    m_console->postCloseMessage();
    if (m_childProcess != NULL)
        CloseHandle(m_childProcess);
    delete [] m_conout.bufferData;
    delete m_console;
    delete m_conout.terminal;
    delete m_consoleInput;
    if (m_consoleMode) {
        delete [] m_conerr.bufferData;
        delete m_conerr.terminal;
    }
}

// Write a "Device Status Report" command to the terminal.  The terminal will
// reply with a row+col escape sequence.  Presumably, the DSR reply will not
// split a keypress escape sequence, so it should be safe to assume that the
// bytes before it are complete keypresses.
void Agent::sendDsr()
{
    m_dataSocket->write("\x1B[6n");
}

NamedPipe *Agent::makeSocket(LPCWSTR pipeName)
{
    NamedPipe *pipe = createNamedPipe();
    if (!pipe->connectToServer(pipeName)) {
        trace("error: could not connect to %ls", pipeName);
        ::exit(1);
    }
    pipe->setReadBufferSize(64 * 1024);
    return pipe;
}

void Agent::resetConsoleTracking(con_status_s &con, bool sendClear)
{
    memset(con.bufferData, 0, sizeof(CHAR_INFO) * BUFFER_LINE_COUNT * MAX_CONSOLE_WIDTH);
    con.syncRow = -1;
    m_console->switchConsoleBuffer(con.type);
    con.scrapedLineCount = m_console->windowRect().top();
    con.scrolledCount = 0;
    con.maxBufferedLine = -1;
    con.dirtyWindowTop = -1;
    con.dirtyLineCount = 0;
    con.terminal->reset(sendClear, con.scrapedLineCount);
}

void Agent::onPipeIo(NamedPipe *namedPipe)
{
    if (namedPipe == m_controlSocket)
        pollControlSocket();
    else if (namedPipe == m_dataSocket)
        pollDataSocket();
}

void Agent::pollControlSocket()
{
    if (m_controlSocket->isClosed()) {
        trace("Agent shutting down");
        shutdown();
        return;
    }

    while (true) {
        int32_t packetSize;
        int size = m_controlSocket->peek((char*)&packetSize, sizeof(int32_t));
        if (size < (int)sizeof(int32_t))
            break;
        int totalSize = sizeof(int32_t) + packetSize;
        if (m_controlSocket->bytesAvailable() < totalSize) {
            if (m_controlSocket->readBufferSize() < totalSize)
                m_controlSocket->setReadBufferSize(totalSize);
            break;
        }
        std::string packetData = m_controlSocket->read(totalSize);
        ASSERT((int)packetData.size() == totalSize);
        ReadBuffer buffer(packetData);
        buffer.getInt(); // Discard the size.
        handlePacket(buffer);
    }
}


void Agent::handlePacket(ReadBuffer &packet)
{
    int type = packet.getInt();
    int32_t result = -1;
    switch (type) {
    case AgentMsg::Ping:
        result = 0;
        break;
    case AgentMsg::StartProcess:
        result = handleStartProcessPacket(packet);
        break;
    case AgentMsg::SetSize:
        result = handleSetSizePacket(packet);
        break;
    case AgentMsg::GetExitCode:
        ASSERT(packet.eof());
        result = m_childExitCode;
        break;
    default:
        trace("Unrecognized message, id:%d", type);
    }
    m_controlSocket->write((char*)&result, sizeof(result));
}

int Agent::handleStartProcessPacket(ReadBuffer &packet)
{
    BOOL success;
    ASSERT(m_childProcess == NULL);

    std::wstring program = packet.getWString();
    std::wstring cmdline = packet.getWString();
    std::wstring cwd = packet.getWString();
    std::wstring env = packet.getWString();
    std::wstring desktop = packet.getWString();
    ASSERT(packet.eof());

    LPCWSTR programArg = program.empty() ? NULL : program.c_str();
    std::vector<wchar_t> cmdlineCopy;
    LPWSTR cmdlineArg = NULL;
    if (!cmdline.empty()) {
        cmdlineCopy.resize(cmdline.size() + 1);
        cmdline.copy(&cmdlineCopy[0], cmdline.size());
        cmdlineCopy[cmdline.size()] = L'\0';
        cmdlineArg = &cmdlineCopy[0];
    }
    LPCWSTR cwdArg = cwd.empty() ? NULL : cwd.c_str();
    LPCWSTR envArg = env.empty() ? NULL : env.data();

    STARTUPINFO sui;
    PROCESS_INFORMATION pi;
    memset(&sui, 0, sizeof(sui));
    memset(&pi, 0, sizeof(pi));
    sui.cb = sizeof(STARTUPINFO);
    sui.lpDesktop = desktop.empty() ? NULL : (LPWSTR)desktop.c_str();
    if (m_consoleMode) {
        sui.dwFlags = STARTF_USESTDHANDLES;
        DuplicateHandle(GetCurrentProcess(), m_console->conin(), GetCurrentProcess(), &sui.hStdInput, 0, TRUE, DUPLICATE_SAME_ACCESS);
        DuplicateHandle(GetCurrentProcess(), m_console->conout(), GetCurrentProcess(), &sui.hStdOutput, 0, TRUE, DUPLICATE_SAME_ACCESS);
        DuplicateHandle(GetCurrentProcess(), m_console->conerr(), GetCurrentProcess(), &sui.hStdError, 0, TRUE, DUPLICATE_SAME_ACCESS);
    }

    success = CreateProcess(programArg, cmdlineArg, NULL, NULL,
                            /*bInheritHandles=*/m_consoleMode,
                            /*dwCreationFlags=*/CREATE_UNICODE_ENVIRONMENT |
                            /*CREATE_NEW_PROCESS_GROUP*/0,
                            (LPVOID)envArg, cwdArg, &sui, &pi);
    int ret = success ? 0 : GetLastError();

    trace("CreateProcess: %s %d",
          (success ? "success" : "fail"),
          (int)pi.dwProcessId);

    if (success) {
        CloseHandle(pi.hThread);
        m_childProcess = pi.hProcess;
    }

    return ret;
}

int Agent::handleSetSizePacket(ReadBuffer &packet)
{
    int cols = packet.getInt();
    int rows = packet.getInt();
    ASSERT(packet.eof());
    resizeWindow(m_conout, cols, rows);
    if (m_consoleMode) resizeWindow(m_conerr, cols, rows);
    return 0;
}

void Agent::pollDataSocket()
{
    m_consoleInput->writeInput(m_dataSocket->readAll());

    // If the child process had exited, then close the data socket if we've
    // finished sending all of the collected output.
    if (m_closingDataSocket &&
            !m_dataSocket->isClosed() &&
            m_dataSocket->bytesToSend() == 0) {
        trace("Closing data pipe after data is sent");
        m_dataSocket->closePipe();
    }
}

void Agent::onPollTimeout()
{
    // Give the ConsoleInput object a chance to flush input from an incomplete
    // escape sequence (e.g. pressing ESC).
    m_consoleInput->flushIncompleteEscapeCode();

    // Check if the child process has exited.
    if (WaitForSingleObject(m_childProcess, 0) == WAIT_OBJECT_0) {
        DWORD exitCode;
        if (GetExitCodeProcess(m_childProcess, &exitCode))
            m_childExitCode = exitCode;
        CloseHandle(m_childProcess);
        m_childProcess = NULL;

        // Close the data socket to signal to the client that the child
        // process has exited.  If there's any data left to send, send it
        // before closing the socket.
        m_closingDataSocket = true;
    }

    // Scrape for output *after* the above exit-check to ensure that we collect
    // the child process's final output.
    if (!m_dataSocket->isClosed())
        scrapeOutput(m_conout);
    if (m_consoleMode && !m_errDataSocket->isClosed())
        scrapeOutput(m_conerr);

    if (m_closingDataSocket) {
        if (!m_dataSocket->isClosed() && m_dataSocket->bytesToSend() == 0) {
            trace("Closing data pipe after child exit");
            m_dataSocket->closePipe();
        }
        if (m_consoleMode && !m_errDataSocket->isClosed() && m_errDataSocket->bytesToSend() == 0) {
            trace("Closing err data pipe after child exit");
            m_errDataSocket->closePipe();
        }
    }
}

// Detect window movement.  If the window moves down (presumably as a
// result of scrolling), then assume that all screen buffer lines down to
// the bottom of the window are dirty.
void Agent::markEntireWindowDirty(con_status_s &con)
{
    m_console->switchConsoleBuffer(con.type);
    SmallRect windowRect = m_console->windowRect();
    con.dirtyLineCount = std::max(con.dirtyLineCount,
                                windowRect.top() + windowRect.height());
}

// Scan the screen buffer and advance the dirty line count when we find
// non-empty lines.
void Agent::scanForDirtyLines(con_status_s &con)
{
    m_console->switchConsoleBuffer(con.type);
    const SmallRect windowRect = m_console->windowRect();
    CHAR_INFO prevChar;
    if (con.dirtyLineCount >= 1) {
        m_console->read(SmallRect(windowRect.width() - 1,
                                  con.dirtyLineCount - 1,
                                  1, 1),
                        &prevChar);
    } else {
        m_console->read(SmallRect(0, 0, 1, 1), &prevChar);
    }
    int attr = prevChar.Attributes;

    for (int line = con.dirtyLineCount;
         line < windowRect.top() + windowRect.height();
         ++line) {
        CHAR_INFO lineData[MAX_CONSOLE_WIDTH]; // TODO: bufoverflow
        SmallRect lineRect(0, line, windowRect.width(), 1);
        m_console->read(lineRect, lineData);
        for (int col = 0; col < windowRect.width(); ++col) {
            int newAttr = lineData[col].Attributes;
            if (lineData[col].Char.AsciiChar != ' ' || attr!= newAttr)
                con.dirtyLineCount = line + 1;
            newAttr = attr;
        }
    }
}

void Agent::resizeWindow(con_status_s &con, int cols, int rows)
{
    m_console->switchConsoleBuffer(con.type);
    freezeConsole();

    Coord bufferSize = m_console->bufferSize();
    SmallRect windowRect = m_console->windowRect();
    Coord newBufferSize(cols, bufferSize.Y);
    SmallRect newWindowRect;

    // This resize behavior appears to match what happens when I resize the
    // console window by hand.
    if (windowRect.top() + windowRect.height() == bufferSize.Y ||
            windowRect.top() + rows >= bufferSize.Y) {
        // Lock the bottom of the new window to the bottom of the buffer if either
        //  - the window was already at the bottom of the buffer, OR
        //  - there isn't enough room.
        newWindowRect = SmallRect(0, newBufferSize.Y - rows, cols, rows);
    } else {
        // Keep the top of the window where it is.
        newWindowRect = SmallRect(0, windowRect.top(), cols, rows);
    }

    if (con.dirtyWindowTop != -1 && con.dirtyWindowTop < windowRect.top())
        markEntireWindowDirty(con);
    con.dirtyWindowTop = newWindowRect.top();

    m_console->reposition(newBufferSize, newWindowRect);
    unfreezeConsole();
}

void Agent::scrapeOutput(con_status_s &con)
{
    m_console->switchConsoleBuffer(con.type);
    freezeConsole();

    const Coord cursor = m_console->cursorPosition();
    const SmallRect windowRect = m_console->windowRect();

    if (con.syncRow != -1) {
        // If a synchronizing marker was placed into the history, look for it
        // and adjust the scroll count.
        int markerRow = findSyncMarker(con);
        if (markerRow == -1) {
            // Something has happened.  Reset the terminal.
            trace("Sync marker has disappeared -- resetting the terminal");
            resetConsoleTracking(con);
        } else if (markerRow != con.syncRow) {
            ASSERT(markerRow < con.syncRow);
            con.scrolledCount += (con.syncRow - markerRow);
            con.syncRow = markerRow;
            // If the buffer has scrolled, then the entire window is dirty.
            markEntireWindowDirty(con);
        }
    }

    // Update the dirty line count:
    //  - If the window has moved, the entire window is dirty.
    //  - Everything up to the cursor is dirty.
    //  - All lines above the window are dirty.
    //  - Any non-blank lines are dirty.
    if (con.dirtyWindowTop != -1) {
        if (windowRect.top() > con.dirtyWindowTop) {
            // The window has moved down, presumably as a result of scrolling.
            markEntireWindowDirty(con);
        } else if (windowRect.top() < con.dirtyWindowTop) {
            // The window has moved upward.  This is generally not expected to
            // happen, but the CMD/PowerShell CLS command will move the window
            // to the top as part of clearing everything else in the console.
            trace("Window moved upward -- resetting the terminal");
            resetConsoleTracking(con);
        }
    }
    con.dirtyWindowTop = windowRect.top();
    con.dirtyLineCount = std::max(con.dirtyLineCount, cursor.Y + 1);
    con.dirtyLineCount = std::max(con.dirtyLineCount, (int)windowRect.top());
    scanForDirtyLines(con);

    // Note that it's possible for all the lines on the current window to
    // be non-dirty.

    int firstLine = std::min(con.scrapedLineCount,
                             windowRect.top() + con.scrolledCount);
    int stopLine = std::min(con.dirtyLineCount,
                            windowRect.top() + windowRect.height()) +
            con.scrolledCount;

    bool sawModifiedLine = false;

    for (int line = firstLine; line < stopLine; ++line) {
        CHAR_INFO curLine[MAX_CONSOLE_WIDTH]; // TODO: bufoverflow
        const int w = windowRect.width();
        m_console->read(SmallRect(0, line - con.scrolledCount, w, 1), curLine);

        // TODO: The memcpy can overflow the m_bufferData buffer.
        CHAR_INFO (&bufLine)[MAX_CONSOLE_WIDTH] =
                con.bufferData[line % BUFFER_LINE_COUNT];
        if (sawModifiedLine ||
                line > con.maxBufferedLine ||
                memcmp(curLine, bufLine, sizeof(CHAR_INFO) * w) != 0) {
            //trace("sent line %d", line);
            con.terminal->sendLine(line, curLine, windowRect.width());
            memset(bufLine, 0, sizeof(bufLine));
            memcpy(bufLine, curLine, sizeof(CHAR_INFO) * w);
            for (int col = w; col < MAX_CONSOLE_WIDTH; ++col) {
                bufLine[col].Attributes = curLine[w - 1].Attributes;
                bufLine[col].Char.AsciiChar = ' ';
            }
            con.maxBufferedLine = std::max(con.maxBufferedLine, line);
            sawModifiedLine = true;
        }
    }

    con.scrapedLineCount = windowRect.top() + con.scrolledCount;

    if (windowRect.top() > 200) { // TODO: replace hard-coded constant
        createSyncMarker(con, windowRect.top() - 200);
    }

    con.terminal->finishOutput(std::pair<int, int>(cursor.X,
                                                 cursor.Y + con.scrolledCount));

    unfreezeConsole();
}

void Agent::freezeConsole()
{
    SendMessage(m_console->hwnd(), WM_SYSCOMMAND, SC_CONSOLE_SELECT_ALL, 0);
}

void Agent::unfreezeConsole()
{
    SendMessage(m_console->hwnd(), WM_CHAR, 27, 0x00010001);
}

void Agent::syncMarkerText(con_status_s con, CHAR_INFO *output)
{
    char str[SYNC_MARKER_LEN + 1];// TODO: use a random string
    sprintf(str, "S*Y*N*C*%08x", con.syncCounter);
    memset(output, 0, sizeof(CHAR_INFO) * SYNC_MARKER_LEN);
    for (int i = 0; i < SYNC_MARKER_LEN; ++i) {
        output[i].Char.AsciiChar = str[i];
        output[i].Attributes = 7;
    }
}

int Agent::findSyncMarker(con_status_s con)
{
    m_console->switchConsoleBuffer(con.type);
    ASSERT(con.syncRow >= 0);
    CHAR_INFO marker[SYNC_MARKER_LEN];
    CHAR_INFO column[BUFFER_LINE_COUNT];
    syncMarkerText(con, marker);
    SmallRect rect(0, 0, 1, con.syncRow + SYNC_MARKER_LEN);
    m_console->read(rect, column);
    int i;
    for (i = con.syncRow; i >= 0; --i) {
        int j;
        for (j = 0; j < SYNC_MARKER_LEN; ++j) {
            if (column[i + j].Char.AsciiChar != marker[j].Char.AsciiChar)
                break;
        }
        if (j == SYNC_MARKER_LEN)
            return i;
    }
    return -1;
}

void Agent::createSyncMarker(con_status_s con, int row)
{
    m_console->switchConsoleBuffer(con.type);
    // Write a new marker.
    con.syncCounter++;
    CHAR_INFO marker[SYNC_MARKER_LEN];
    syncMarkerText(con, marker);
    con.syncRow = row;
    SmallRect markerRect(0, con.syncRow, 1, SYNC_MARKER_LEN);
    m_console->write(markerRect, marker);
}
