#ifndef AGENTMSG_H
#define AGENTMSG_H

struct AgentMsg
{
    enum Type {
        StartProcess,
        SetSize
    };
};

#endif // AGENTMSG_H
