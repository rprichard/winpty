#define WINVER 0x501
#include "AgentClient.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <QCoreApplication>
#include <QtDebug>
#include <windows.h>

// Note that this counter makes Agentclient non-thread-safe.
int AgentClient::m_counter = 0;

AgentClient::AgentClient(QObject *parent) :
    QObject(parent)
{
    // Start a named pipe server.
    m_socketServer = new QLocalServer(this);
    QString serverName =
            "ConsoleAgent-" +
            QString::number(QCoreApplication::applicationPid()) + "-" +
            QString::number(++m_counter);
    m_socketServer->listen(serverName);

    // TODO: Improve this code.  If we're in the release subdirectory,
    // find the release Agent.  Look in the same directory first.
    QString agentProgram = QCoreApplication::applicationDirPath() + "\\..\\..\\Agent-build-desktop\\debug\\Agent.exe";
    QString agentCmdLine = agentProgram + " " + m_socketServer->fullServerName();

    // Start the agent.
    BOOL success;
    STARTUPINFO sui;
    memset(&sui, 0, sizeof(sui));
    sui.cb = sizeof(sui);
    sui.dwFlags = STARTF_USESHOWWINDOW;
    sui.wShowWindow = SW_SHOW; // TODO: put this back --- SW_HIDE;
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    m_agentProcess = new PROCESS_INFORMATION;
    QVector<wchar_t> cmdline(agentCmdLine.size() + 1);
    agentCmdLine.toWCharArray(cmdline.data());
    success = CreateProcess(
        (LPCWSTR)agentProgram.utf16(),
        cmdline.data(),
        NULL, NULL,
        /*bInheritHandles=*/FALSE,
        /*dwCreationFlags=*/CREATE_NEW_CONSOLE,
        NULL, NULL,
        &sui, m_agentProcess);
    if (!success)
        qFatal("Could not start agent subprocess.");
    qDebug("New child process: PID %d", (int)m_agentProcess->dwProcessId);

    if (!m_socketServer->waitForNewConnection(30000))
        qFatal("Child process did not connect to parent pipe server.");
    m_socket = m_socketServer->nextPendingConnection();
    Q_ASSERT(m_socket != NULL);
}

/*
void AgentClient::sendKeyPress(QKeyEvent *event)
{
    // TODO: Flesh this out....  Also: this code is intended
    // to be portable across operating systems, so using
    // nativeVirtualKey is wrong.
    if (event->nativeVirtualKey() != 0) {
        INPUT_RECORD ir;
        memset(&ir, 0, sizeof(ir));
        ir.EventType = KEY_EVENT;
        ir.Event.KeyEvent.bKeyDown = TRUE;
        ir.Event.KeyEvent.wVirtualKeyCode = event->nativeVirtualKey();
        ir.Event.KeyEvent.wVirtualScanCode = event->nativeScanCode();
        ir.Event.KeyEvent.uChar.UnicodeChar =
                event->text().isEmpty() ? L'\0' : event->text().at(0).unicode();
        ir.Event.KeyEvent.wRepeatCount = event->count();
        m_socket->write((char*)&ir, sizeof(ir));
    }
}

void AgentClient::sendKeyRelease(QKeyEvent *event)
{

}
*/

void AgentClient::writeInputRecord(INPUT_RECORD *ir)
{
    m_socket->write((char*)ir, sizeof(*ir));
}

int AgentClient::agentPid()
{
    return m_agentProcess->dwProcessId;
}

void AgentClient::startShell()
{
    FreeConsole();
    AttachConsole(agentPid());

    HANDLE conout1, conout2, conin;

    {
        // TODO: Should the permissions be more restrictive?
        // TODO: Is this code necessary or even desirable?  If I
        // don't change these handles, then are the old values
        // invalid?  If so, what happens if I create a console
        // subprocess?
        // The handle must be inheritable.  See comment below.
        SECURITY_ATTRIBUTES sa;
        memset(&sa, 0, sizeof(sa));
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        conout1 = CreateFile(L"CONOUT$",
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa,
            OPEN_EXISTING,
            0, NULL);
        conout2 = CreateFile(L"CONOUT$",
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa,
            OPEN_EXISTING,
            0, NULL);
        conin = CreateFile(L"CONIN$",
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa,
            OPEN_EXISTING,
            0, NULL);
        Q_ASSERT(conin != NULL);
        Q_ASSERT(conout1 != NULL);
        Q_ASSERT(conout2 != NULL);
        /*
        BOOL success;
        success = SetStdHandle(STD_OUTPUT_HANDLE, conout1);
        Q_ASSERT(success);
        success = SetStdHandle(STD_ERROR_HANDLE, conout2);
        Q_ASSERT(success);
        success = SetStdHandle(STD_INPUT_HANDLE, conin);
        Q_ASSERT(success);
        */
    }

    {
        wchar_t program[80] = L"c:\\windows\\system32\\cmd.exe";
        wchar_t cmdline[80];
        wcscpy(cmdline, program);
        STARTUPINFO sui;
        memset(&sui, 0, sizeof(sui));
        sui.cb = sizeof(sui);
        sui.dwFlags = STARTF_USESTDHANDLES;
        sui.hStdInput = conin;
        sui.hStdOutput = conout1;
        sui.hStdError = conout2;
        PROCESS_INFORMATION pi;
        memset(&pi, 0, sizeof(pi));
        BOOL success = CreateProcess(
                    program,
                    cmdline,
                    NULL,
                    NULL,
                    FALSE,
                    0,
                    NULL,
                    NULL,
                    &sui,
                    &pi);
        if (!success) {
            qDebug("Could not start shell");
        }
    }

    CloseHandle(conout1);
    CloseHandle(conout2);
    CloseHandle(conin);

    FreeConsole();
}
