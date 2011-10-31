#include "ConsoleWindow.h"
#include "ui_ConsoleWindow.h"
#include "AgentClient.h"
#include <QProcess>
#include <QtDebug>
#include <stdio.h>

#define WINVER 0x501
#include <windows.h>

ConsoleWindow::ConsoleWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ConsoleWindow)
{
    ui->setupUi(this);
    m_agentClient = new AgentClient(this);
    ui->widget->initWithAgent(m_agentClient);
    startShell();
}

ConsoleWindow::~ConsoleWindow()
{
    delete ui;
}


void ConsoleWindow::startShell()
{
    AttachConsole(m_agentClient->agentPid());

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
