#ifndef AGENTMSG_H
#define AGENTMSG_H

struct AgentMsg
{
    enum Type {
        StartProcess,
        SetSize,
        GetExitCode
    };
};

#endif // AGENTMSG_H
