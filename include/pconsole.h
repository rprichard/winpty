#ifndef PCONSOLE_H
#define PCONSOLE_H

#include <stdlib.h>
#include <windows.h>

#ifdef PCONSOLE
#define PCONSOLE_API __declspec(dllexport)
#else
#define PCONSOLE_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pconsole_s pconsole_t;
typedef void (*pconsole_io_cb)(pconsole_t *pconsole);
typedef void (*pconsole_process_exit_cb)(pconsole_t *pconsole,
					 int pid,
					 int exitcode);

/*
 * pconsole API.
 *
 * This library provides non-blocking I/O APIs that communicate with a pconsole
 * agent process over Win32 named pipes.
 *
 * Most I/O is transferred with the pconsole_read and pconsole_write functions.
 * The format is the same as a Unix pty (i.e. escape sequences).
 *
 * A pconsole_t instance has an internal write buffer that grows on-demand to
 * an arbitrary size.  A pconsole client should use the
 * pconsole_get_output_queue_size API to limit the size of the output buffer.
 * (TODO: This isn't a practical concern, though, is it?)  The internal read
 * buffer is statically-sized -- if the client does not read available data
 * using pconsole_read, then the pipe will eventually block.
 *
 * The library uses callbacks to notify the client.  A callback function is
 * always called on a worker thread that the pconsole library handles.
 */

/*
 * Starts a new pconsole instance with the given size.
 */
PCONSOLE_API pconsole_t *pconsole_open(int cols, int rows);

/*
 * Start a child process.  Either (but not both) of program and cmdline may
 * be NULL.  cwd and env may be NULL.  env is a pointer to a NULL-terminated
 * array of strings.
 *
 * This function never modifies the cmdline, unlike CreateProcess.
 *
 * Only one child process may be started.  After the child process exits, the
 * agent will flush and close the data pipe.
 */
PCONSOLE_API int pconsole_start_process(pconsole_t *pc,
					const wchar_t *program,
					const wchar_t *cmdline,
					const wchar_t *cwd,
					const wchar_t *const *env);

PCONSOLE_API int pconsole_get_exit_code(pconsole_t *pc);

PCONSOLE_API int pconsole_flush_and_close(pconsole_t *pc);

/*
 * Returns an overlapped-mode pipe handle that can be read and written
 * like a Unix terminal.
 */
PCONSOLE_API HANDLE pconsole_get_data_pipe(pconsole_t *pc);

/*
 * Change the size of the Windows console.
 */
PCONSOLE_API int pconsole_set_size(pconsole_t *pc, int cols, int rows);

/*
 * Closes the pconsole.
 */
PCONSOLE_API void pconsole_close(pconsole_t *pc);

#ifdef __cplusplus
}
#endif

#endif /* PCONSOLE_H */
