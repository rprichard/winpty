#include "ConsoleWindow.h"
#include "ui_ConsoleWindow.h"
#include "../Shared/AgentClient.h"
#include <QProcess>
#include <QtDebug>
#include <stdio.h>

ConsoleWindow::ConsoleWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ConsoleWindow)
{
    ui->setupUi(this);
    m_agentClient = new AgentClient(80, 25, this);
    ui->widget->initWithAgent(m_agentClient);
    m_agentClient->startShell();
}

ConsoleWindow::~ConsoleWindow()
{
    delete ui;
}
