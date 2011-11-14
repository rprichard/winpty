#ifndef AGENTMSG_H
#define AGENTMSG_H

#include <windows.h>

class AgentMsg
{
public:
    enum Type {
        InputRecord,
        WindowSize
    };

    Type type;
    union {
        INPUT_RECORD inputRecord;
        struct {
            unsigned short cols;
            unsigned short rows;
        } windowSize;
    } u;
};

#endif // AGENTMSG_H
