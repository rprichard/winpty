#ifndef AGENTMSG_H
#define AGENTMSG_H

struct AgentMsg
{
    enum Type {
        Ping,
        StartProcess,
        SetSize,
        GetExitCode
    };
};

#endif // AGENTMSG_H
