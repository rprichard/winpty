#ifndef CONSOLEAGENT_H
#define CONSOLEAGENT_H

#include <windows.h>

struct ConsoleAgent
{
    HANDLE mutex;
    HANDLE event;
    CHAR_INFO data[256 * 256];

};

#endif // CONSOLEAGENT_H
