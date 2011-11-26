#ifndef AGENTMSG_H
#define AGENTMSG_H

#include <windows.h>

class AgentMsg
{
public:
    enum Type {
        InputRecord,
        WindowSize,
        SetAutoShutDownFlag
    };

    Type type;
    union {
        INPUT_RECORD inputRecord;
        struct {
            unsigned short cols;
            unsigned short rows;
        } windowSize;
        BOOL flag;
    } u;
};

#endif // AGENTMSG_H
