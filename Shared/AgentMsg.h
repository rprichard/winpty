#ifndef AGENTMSG_H
#define AGENTMSG_H

#include <windows.h>

struct AgentMsg
{
    enum Type {
        StartProcess,
        SetSize
    };
};

#endif // AGENTMSG_H
