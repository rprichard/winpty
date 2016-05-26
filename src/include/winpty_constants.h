/*
 * Copyright (c) 2016 Ryan Prichard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef WINPTY_CONSTANTS_H
#define WINPTY_CONSTANTS_H

// TODO: Review the winpty and spawn flags...

/*
 * You may want to include winpty.h instead, which includes this header.
 *
 * This file is split out from winpty.h so that the agent can access the
 * winpty flags without also declaring the libwinpty APIs.
 */

/*****************************************************************************
 * Error codes. */

#define WINPTY_ERROR_SUCCESS                        0
#define WINPTY_ERROR_OUT_OF_MEMORY                  1
#define WINPTY_ERROR_SPAWN_CREATE_PROCESS_FAILED    2
#define WINPTY_ERROR_LOST_CONNECTION                3
#define WINPTY_ERROR_AGENT_EXE_MISSING              4
#define WINPTY_ERROR_UNSPECIFIED                    5
#define WINPTY_ERROR_AGENT_DIED                     6
#define WINPTY_ERROR_AGENT_TIMEOUT                  7
#define WINPTY_ERROR_AGENT_CREATION_FAILED          8



/*****************************************************************************
 * Configuration of a new agent. */

#define WINPTY_FLAG_MASK 0ull



/*****************************************************************************
 * winpty agent RPC call: process creation. */

/* If the spawn is marked "auto-shutdown", then the agent shuts down console
 * output once the process exits.  The agent stops polling for new console
 * output, and once all pending data has been written to the output pipe, the
 * agent closes the pipe.  (At that point, the pipe may still have data in it,
 * which the client may read.  Once all the data has been read, further reads
 * return EOF.) */
#define WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN 1ull

/* All the spawn flags. */
#define WINPTY_SPAWN_FLAG_MASK (0ull \
    | WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN \
)



#endif /* WINPTY_CONSTANTS_H */
