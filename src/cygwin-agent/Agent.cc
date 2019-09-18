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

#include "Agent.h"

#include <windows.h>

#include <stdint.h>
#include <sys/ioctl.h>

#include <string>
#include <vector>

#include "../include/winpty_constants.h"

#include "../shared/AgentMsg.h"
#include "../shared/Buffer.h"
#include "../shared/DebugClient.h"
#include "../shared/GenRandom.h"
#include "../shared/StringBuilder.h"
#include "../shared/StringUtil.h"
#include "../shared/WinptyAssert.h"

#include "AgentCygwinPty.h"
#include "NamedPipe.h"

// (from src/agent/Scraper.h)
const int MAX_CONSOLE_HEIGHT = 2000, MAX_CONSOLE_WIDTH = 2500;

namespace {

static HANDLE duplicateHandle(HANDLE h) {
    HANDLE ret = nullptr;
    if (!DuplicateHandle(
            GetCurrentProcess(), h,
            GetCurrentProcess(), &ret,
            0, FALSE, DUPLICATE_SAME_ACCESS)) {
        ASSERT(false && "DuplicateHandle failed!");
    }
    return ret;
}

// It's safe to truncate a handle from 64-bits to 32-bits, or to sign-extend it
// back to 64-bits.  See the MSDN article, "Interprocess Communication Between
// 32-bit and 64-bit Applications".
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa384203.aspx
static int64_t int64FromHandle(HANDLE h) {
    return static_cast<int64_t>(reinterpret_cast<intptr_t>(h));
}

static inline WriteBuffer newPacket() {
    WriteBuffer packet;
    packet.putRawValue<uint64_t>(0); // Reserve space for size.
    return packet;
}

} // anonymous namespace

Agent::Agent(LPCWSTR controlPipeName,
             uint64_t agentFlags,
             int mouseMode,
             int initialCols,
             int initialRows) :
    m_useConerr((agentFlags & WINPTY_FLAG_CONERR) != 0),
    m_plainMode((agentFlags & WINPTY_FLAG_PLAIN_OUTPUT) != 0),
    m_mouseMode(mouseMode)
{
    trace("Agent::Agent entered");

    ASSERT(initialCols >= 1 && initialRows >= 1);
    initialCols = std::min(initialCols, MAX_CONSOLE_WIDTH);
    initialRows = std::min(initialRows, MAX_CONSOLE_HEIGHT);
    m_pty_winp.ws_row = initialRows;
    m_pty_winp.ws_col = initialCols;
    m_pty = std::unique_ptr<AgentCygwinPty>(new AgentCygwinPty());

    m_controlPipe = &connectToControlPipe(controlPipeName);
    m_coninPipe = &createDataServerPipe(false, L"conin");
    m_conoutPipe = &createDataServerPipe(true, L"conout");
    if (m_useConerr) {
        m_conerrPipe = &createDataServerPipe(true, L"conerr");
    }

    // Send an initial response packet to winpty.dll containing pipe names.
    {
        auto setupPacket = newPacket();
        setupPacket.putWString(m_coninPipe->name());
        setupPacket.putWString(m_conoutPipe->name());
        if (m_useConerr) {
            setupPacket.putWString(m_conerrPipe->name());
        }
        writePacket(setupPacket);
    }
}

Agent::~Agent()
{
    trace("Agent::~Agent entered");
    trace("Closing CONOUT pipe (auto-shutdown)");
    m_conoutPipe->closePipe();
    if (m_conerrPipe != nullptr) {
        trace("Closing CONERR pipe (auto-shutdown)");
        m_conerrPipe->closePipe();
    }
    shutdown();
    agentShutdown();
    if (m_childProcess != NULL) {
        CloseHandle(m_childProcess);
    }
    m_pty.reset();
}

NamedPipe &Agent::connectToControlPipe(LPCWSTR pipeName)
{
    NamedPipe &pipe = createNamedPipe();
    pipe.connectToServer(pipeName, NamedPipe::OpenMode::Duplex);
    pipe.setReadBufferSize(64 * 1024);
    return pipe;
}

// Returns a new server named pipe.  It has not yet been connected.
NamedPipe &Agent::createDataServerPipe(bool write, const wchar_t *kind)
{
    const auto name =
        (WStringBuilder(128)
            << L"\\\\.\\pipe\\winpty-"
            << kind << L'-'
            << GenRandom().uniqueName()).str_moved();
    NamedPipe &pipe = createNamedPipe();
    pipe.openServerPipe(
        name.c_str(),
        write ? NamedPipe::OpenMode::Writing
              : NamedPipe::OpenMode::Reading,
        write ? 8192 : 0,
        write ? 0 : 256);
    if (!write) {
        pipe.setReadBufferSize(64 * 1024);
    }
    return pipe;
}

HANDLE Agent::getPtyEventHandle()
{
    if (m_pty) {
        return m_pty->m_inputWorker.m_event.get();
    } else {
        return nullptr;
    }
}

void Agent::handleGetConsoleProcessListPacket(ReadBuffer &packet)
{
    packet.assertEof();

    auto processList = std::vector<DWORD>(1);
    auto processCount = 1u;

    // FIXME
    processList[0] = m_pty->m_pid;
    auto reply = newPacket();
    reply.putInt32(processCount);
    for (DWORD i = 0; i < processCount; i++) {
        reply.putInt32(processList[i]);
    }
    writePacket(reply);
}

void Agent::handlePacket(ReadBuffer &packet)
{
    const int type = packet.getInt32();
    switch (type) {
    case AgentMsg::StartProcess:
        handleStartProcessPacket(packet);
        break;
    case AgentMsg::SetSize:
        // TODO: I think it might make sense to collapse consecutive SetSize
        // messages.  i.e. The terminal process can probably generate SetSize
        // messages faster than they can be processed, and some GUIs might
        // generate a flood of them, so if we can read multiple SetSize packets
        // at once, we can ignore the early ones.
        handleSetSizePacket(packet);
        break;
    case AgentMsg::GetConsoleProcessList:
        handleGetConsoleProcessListPacket(packet);
        break;
    default:
        trace("Unrecognized message, id:%d", type);
    }
}

void Agent::handleSetSizePacket(ReadBuffer &packet)
{
    const int cols = packet.getInt32();
    const int rows = packet.getInt32();
    packet.assertEof();
    m_pty_winp.ws_col = cols;
    m_pty_winp.ws_row = rows;
    ioctl(m_pty->m_fd, TIOCSWINSZ, &m_pty_winp);
    auto reply = newPacket();
    writePacket(reply);
}

void Agent::handleStartProcessPacket(ReadBuffer &packet)
{
    ASSERT(m_childProcess == nullptr);

    const uint64_t spawnFlags = packet.getInt64();
    const bool wantProcessHandle = packet.getInt32() != 0;
    const bool wantThreadHandle = packet.getInt32() != 0;
    const auto program = packet.getWString();
    const auto cmdline = packet.getWString();
    const auto cwd = packet.getWString();
    const auto env = packet.getWString();
    const auto desktop = packet.getWString();
    packet.assertEof();

    auto cmdlineV = vectorWithNulFromString(cmdline);
    auto desktopV = vectorWithNulFromString(desktop);
    auto envV = vectorFromString(env);

    HANDLE hProcess, hThread;
    int lastError = 0;
    BOOL success = TRUE;

    if ((lastError = m_pty->fork(cmdline, m_pty_winp, &hProcess, &hThread)) != 0) {
        success = FALSE;
    }
    auto reply = newPacket();
    if (success) {
        int64_t replyProcess = 0;
        int64_t replyThread = 0;
        if (wantProcessHandle) {
            replyProcess = int64FromHandle(duplicateHandle(hProcess));
        }
        if (wantThreadHandle) {
            replyThread = int64FromHandle(duplicateHandle(hThread));
        }
        CloseHandle(hThread);
        m_childProcess = hProcess;
        m_autoShutdown = (spawnFlags & WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN) != 0;
        m_exitAfterShutdown = (spawnFlags & WINPTY_SPAWN_FLAG_EXIT_AFTER_SHUTDOWN) != 0;
        reply.putInt32(static_cast<int32_t>(StartProcessResult::ProcessCreated));
        reply.putInt64(replyProcess);
        reply.putInt64(replyThread);
    } else {
        reply.putInt32(static_cast<int32_t>(StartProcessResult::CreateProcessFailed));
        reply.putInt32(lastError);
    }
    writePacket(reply);
}

void Agent::onPipeIo(NamedPipe &namedPipe)
{
    if (&namedPipe == m_coninPipe) {
        pollConinPipe();
    } else if (&namedPipe == m_controlPipe) {
        pollControlPipe();
    }
}

bool Agent::onPtyIo()
{
    const char *buf; size_t buf_len = 0;
    bool rc = false, status;

    if (m_pty) {
        status = m_pty->read(&buf, &buf_len);
        if (buf_len > 0) {
            m_conoutPipe->write(buf, buf_len), rc = true; delete buf;
        }
        if (!status) {
            if (m_childProcess != NULL) {
                CloseHandle(m_childProcess);
            }
            m_pty.reset();
            shutdown(), rc = true;
        }
    }
    return rc;
}

void Agent::pollConinPipe()
{
    if (m_pty) {
        const std::string newData = m_coninPipe->readAllToString();
        m_pty->write(newData.c_str(), newData.size());
    }
}

void Agent::pollControlPipe()
{
    if (m_controlPipe->isClosed()) {
        trace("Agent exiting (control pipe is closed)");
        shutdown();
        return;
    }

    while (true) {
        uint64_t packetSize = 0;
        const auto amt1 =
            m_controlPipe->peek(&packetSize, sizeof(packetSize));
        if (amt1 < sizeof(packetSize)) {
            break;
        }
        ASSERT(packetSize >= sizeof(packetSize) && packetSize <= SIZE_MAX);
        if (m_controlPipe->bytesAvailable() < packetSize) {
            if (m_controlPipe->readBufferSize() < packetSize) {
                m_controlPipe->setReadBufferSize(packetSize);
            }
            break;
        }
        std::vector<char> packetData;
        packetData.resize(packetSize);
        const auto amt2 = m_controlPipe->read(packetData.data(), packetSize);
        ASSERT(amt2 == packetSize);
        try {
            ReadBuffer buffer(std::move(packetData));
            buffer.getRawValue<uint64_t>(); // Discard the size.
            handlePacket(buffer);
        } catch (const ReadBuffer::DecodeError&) {
            ASSERT(false && "Decode error");
        }
    }
}

void Agent::writePacket(WriteBuffer &packet)
{
    const auto &bytes = packet.buf();
    packet.replaceRawValue<uint64_t>(0, bytes.size());
    m_controlPipe->write(bytes.data(), bytes.size());
}
