#ifndef AGENTMESSAGES_H
#define AGENTMESSAGES_H

struct AgentMessage
{
    int size;    // size of message, including this size field
    char data[];
};

#endif // AGENTMESSAGES_H
