#ifndef CONSOLE_H
#define CONSOLE_H

class Console
{
public:
    Console();
    void start();

private:
    HANDLE m_pipeToAgent;
    HANDLE m_pipeFromAgent;
};

#endif // CONSOLE_H
