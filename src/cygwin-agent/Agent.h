// Copyright (c) 2011-2015 Ryan Prichard
// Copyright (c) 2019 Lucio Andrés Illanes Albornoz <lucio@lucioillanes.de>
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

#ifndef AGENT_H
#define AGENT_H

#include <windows.h>

#include <stdint.h>
#include <termios.h>

#include <memory>
#include <string>

#include "../agent/DsrSender.h"
#include "EventLoop.h"

class AgentCygwinPty;
class NamedPipe;
class ReadBuffer;
class WriteBuffer;

class Agent : public EventLoop, public DsrSender
{
public:
    Agent(LPCWSTR controlPipeName,
          uint64_t agentFlags,
          int mouseMode,
          int initialCols,
          int initialRows);
    virtual ~Agent();

private:
    NamedPipe &connectToControlPipe(LPCWSTR pipeName);
    NamedPipe &createDataServerPipe(bool write, const wchar_t *kind);

private:
    void pollControlPipe();
    void handlePacket(ReadBuffer &packet);
    void writePacket(WriteBuffer &packet);
    void handleStartProcessPacket(ReadBuffer &packet);
    void handleSetSizePacket(ReadBuffer &packet);
    void handleGetConsoleProcessListPacket(ReadBuffer &packet);
    void pollConinPipe();

protected:
    virtual HANDLE getPtyEventHandle() override;
    virtual bool onPtyIo() override;
    virtual void onPipeIo(NamedPipe &namedPipe) override;

private:
    const bool m_useConerr;
    const bool m_plainMode;
    const int m_mouseMode;
    NamedPipe *m_controlPipe = nullptr;
    NamedPipe *m_coninPipe = nullptr;
    NamedPipe *m_conoutPipe = nullptr;
    NamedPipe *m_conerrPipe = nullptr;
    bool m_autoShutdown = false;
    bool m_exitAfterShutdown = false;
    bool m_closingOutputPipes = false;
    HANDLE m_childProcess = nullptr;
    std::unique_ptr<AgentCygwinPty> m_pty;
    struct winsize m_pty_winp;
};

#endif // AGENT_H
