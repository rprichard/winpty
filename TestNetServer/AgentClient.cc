#include "AgentClient.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <QCoreApplication>
#include <QDir>
#include <QtDebug>
#include <windows.h>
#include "../Shared/DebugClient.h"
#include "../Shared/AgentMsg.h"

#define AGENT_EXE "pconsole-agent.exe"

// TODO: Note that this counter makes AgentClient non-thread-safe.
// TODO: So does the various API calls that change the process state.
// TODO: (e.g. AttachConsole, FreeConsole)
int AgentClient::m_counter = 0;

AgentClient::AgentClient(int initialCols, int initialRows, QObject *parent) :
    QObject(parent)
{
    BOOL success;

    // Start a named pipe server.
    QLocalServer *socketServer = new QLocalServer(this);
    QString serverName =
            "pconsole-" +
            QString::number(QCoreApplication::applicationPid()) + "-" +
            QString::number(++m_counter);
    socketServer->listen(serverName);

    QDir progDir(QCoreApplication::applicationDirPath());
    QString agentProgram;
    if (progDir.exists(AGENT_EXE)) {
        // The agent might be in the same directory as the server when it's
        // installed.
        agentProgram = progDir.filePath(AGENT_EXE);
    } else {
        // The development directory structure looks like this:
        //     root/
        //         agent/
        //             pconsole-agent.exe
        //         TestNetServer/
        //             testnet-server.exe
        agentProgram = progDir.filePath("../agent/"AGENT_EXE);
    }
    agentProgram = QDir(agentProgram).canonicalPath();

    QString agentCmdLine =
            QString("\"%1\" %2 %3 %4").arg(agentProgram,
                                           socketServer->fullServerName())
                                      .arg(initialCols).arg(initialRows);

    Trace("Starting Agent: [%s]", agentCmdLine.toStdString().c_str());

    // Get a non-interactive window station for the agent.
    // TODO: review security w.r.t. windowstation and desktop.
    HWINSTA originalStation = GetProcessWindowStation();
    HWINSTA station = CreateWindowStation(NULL, 0, WINSTA_ALL_ACCESS, NULL);
    success = SetProcessWindowStation(station);
    Q_ASSERT(success);
    HDESK desktop = CreateDesktop(L"Default", NULL, NULL, 0, GENERIC_ALL, NULL);
    Q_ASSERT(originalStation != NULL);
    Q_ASSERT(station != NULL);
    Q_ASSERT(desktop != NULL);
    wchar_t stationNameWStr[256];
    success = GetUserObjectInformation(station, UOI_NAME,
                                       stationNameWStr, sizeof(stationNameWStr),
                                       NULL);
    Q_ASSERT(success);
    QString stationName = QString::fromWCharArray(stationNameWStr);
    QString startupDesktop = stationName + "\\Default";

    // Start the agent.
    STARTUPINFO sui;
    memset(&sui, 0, sizeof(sui));
    sui.cb = sizeof(sui);
    sui.lpDesktop = (LPWSTR)startupDesktop.utf16();
    m_agentProcess = new PROCESS_INFORMATION;
    QVector<wchar_t> cmdline(agentCmdLine.size() + 1);
    agentCmdLine.toWCharArray(cmdline.data());
    cmdline[agentCmdLine.size()] = L'\0';
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

    if (!socketServer->waitForNewConnection(30000))
        qFatal("Child process did not connect to parent pipe server.");
    m_socket = socketServer->nextPendingConnection();
    Q_ASSERT(m_socket != NULL);
    // TODO: security -- Do we need to verify that this pipe connection was
    // made by the right client?  i.e. The Agent.exe process we just started?
    socketServer->close();

    // Restore the original window station.
    success = SetProcessWindowStation(originalStation);
    Q_ASSERT(success);
    success = CloseDesktop(desktop);
    Q_ASSERT(success);
    success = CloseWindowStation(station);
    Q_ASSERT(success);
}

void AgentClient::writeMsg(const AgentMsg &msg)
{
    m_socket->write((char*)&msg, sizeof(msg));
}

int AgentClient::agentPid()
{
    return m_agentProcess->dwProcessId;
}

void AgentClient::startShell()
{
    if (!FreeConsole())
        Trace("FreeConsole failed");
    if (!AttachConsole(agentPid()))
        Trace("AttachConsole to pid %d failed", agentPid());

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
        if (success)
            Trace("Started shell pid %d", pi.dwProcessId);
        else
            Trace("Could not start shell");
    }

    CloseHandle(conout1);
    CloseHandle(conout2);
    CloseHandle(conin);

    FreeConsole();

    // Now that the shell is started, tell the agent to shutdown when the
    // console has no more programs using it.
    AgentMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = AgentMsg::SetAutoShutDownFlag;
    msg.u.flag = TRUE;
    writeMsg(msg);
}

QLocalSocket *AgentClient::getSocket()
{
    return m_socket;
}
