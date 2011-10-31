#include "AgentIo.h"
#include <assert.h>

// The shared memory is initialized on the client.
void AgentSharedMemory::Init()
{
    BOOL success;

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    requestEvent = CreateEvent(&sa, /*bManualReset=*/FALSE, /*bInitialState=*/FALSE, NULL);
    replyEvent = CreateEvent(&sa, /*bManualReset=*/FALSE, /*bInitialState=*/FALSE, NULL);
    assert(requestEvent != NULL && replyEvent != NULL);

    clientProcess = NULL;
    success = DuplicateHandle(
                GetCurrentProcess(),
                GetCurrentProcess(),
                GetCurrentProcess(),
                &clientProcess,
                0, TRUE, DUPLICATE_SAME_ACCESS);
    assert(success);
}
