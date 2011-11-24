#ifndef CONSOLEWINDOW_H
#define CONSOLEWINDOW_H

#include <QMainWindow>

class AgentClient;

namespace Ui {
    class ConsoleWindow;
}

class ConsoleWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ConsoleWindow(QWidget *parent = 0);
    ~ConsoleWindow();

private:
    Ui::ConsoleWindow *ui;
    AgentClient *m_agentClient;
};

#endif // CONSOLEWINDOW_H
