#include "AgentAssert.h"
#include "../Shared/DebugClient.h"
#include <stdlib.h>

// Calling the standard assert() function does not work in the agent because
// the error message would be printed to the console, and the only way the
// user can see the console is via a working agent!  This custom assert
// function instead sends the message to the DebugServer.

void assertFail(const char *file, int line, const char *cond)
{
    Trace("Assertion failed: %s, file %s, line %d",
          cond, file, line);
    abort();
}
