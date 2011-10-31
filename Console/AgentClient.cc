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

int AgentClient::agentPid()
{
    return m_agentProcess->dwProcessId;
}
