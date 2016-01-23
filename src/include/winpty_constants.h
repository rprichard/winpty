/*
 * Copyright (c) 2011-2016 Ryan Prichard
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
#define WINPTY_ERROR_WINDOWS_ERROR                  5
#define WINPTY_ERROR_INTERNAL_ERROR                 6
#define WINPTY_ERROR_AGENT_DIED                     7
#define WINPTY_ERROR_AGENT_TIMEOUT                  8
#define WINPTY_ERROR_AGENT_CREATION_FAILED          9



/*****************************************************************************
 * Configuration of a new agent. */

/* Enable "plain text mode".  In this mode, winpty avoids outputting escape
 * sequences.  It tries to generate output suitable to situations where a full
 * terminal isn't available.  (e.g. an IDE pops up a window for authenticating
 * an SVN connection.) */
#define WINPTY_FLAG_PLAIN_TEXT 1

/* On XP and Vista, winpty needs to put the hidden console on a desktop in a
 * service window station so that its polling does not interfere with other
 * (visible) console windows.  To create this desktop, it must change the
 * process' window station (i.e. SetProcessWindowStation) for the duration of
 * the winpty_open call.  In theory, this change could interfere with the
 * winpty client (e.g. other threads, spawning children), so winpty by default
 * tasks a special agent with creating the hidden desktop.  Spawning processes
 * on Windows is slow, though, so if WINPTY_FLAG_ALLOW_CURPROC_DESKTOP_CREATION
 * is set, winpty changes this process' window station instead.
 * See https://github.com/rprichard/winpty/issues/58. */
#define WINPTY_FLAG_ALLOW_CURPROC_DESKTOP_CREATION 2

/* Ordinarilly, the agent closes its attached console as it exits, which
 * prompts Windows to kill all the processes attached to the console.  Specify
 * this flag to suppress this behavior. */
#define WINPTY_FLAG_LEAVE_CONSOLE_OPEN_ON_EXIT 4

/* All the agent creation flags. */
#define WINPTY_FLAG_MASK (0 \
    | WINPTY_FLAG_PLAIN_TEXT \
    | WINPTY_FLAG_ALLOW_CURPROC_DESKTOP_CREATION \
    | WINPTY_FLAG_LEAVE_CONSOLE_OPEN_ON_EXIT \
)



/*****************************************************************************
 * winpty agent RPC call: process creation. */

/* If the spawn is marked "auto-shutdown", then the agent shuts down console
 * output once the process exits.  See winpty_shutdown_output. */
#define WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN 1

/* All the spawn flags. */
#define WINPTY_SPAWN_FLAG_MASK (0 \
    | WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN \
)



#endif /* WINPTY_CONSTANTS_H */
