#ifndef PCONSOLE_H
#define PCONSOLE_H

#include <stdlib.h>

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
 * Sets the I/O callback, which may be NULL.  The callback is called:
 *  - after new data is available for reading, AND
 *  - after the output queue size decreases
 */
PCONSOLE_API void pconsole_set_io_cb(pconsole_t *pconsole, pconsole_io_cb cb);

/*
 * Sets the process exit callback, which may be NULL.  The callback is
 * called when a process started with pconsole_start_process exits.
 */
PCONSOLE_API void pconsole_set_process_exit_cb(pconsole_t *pconsole,
					       pconsole_process_exit_cb cb);

/*
 * Start a child process.  Either (but not both) of program and cmdline may
 * be NULL.  cwd and env may be NULL.  env is a pointer to a NULL-terminated
 * array of strings.
 *
 * All the strings are in UTF-8 format.  They are converted to UTF-16 in Win32
 * API calls.
 *
 * This function never modifies the cmdline, unlike CreateProcess.
 *
 * Only one child process may be started.  After the child process exits, the
 * agent will flush and close its output buffer.
 */
PCONSOLE_API int pconsole_start_process(pconsole_t *pconsole,
					const char *program,
					const char *cmdline,
					const char *cwd,
					const char *const *env);

/*
 * Reads pty-like input.  Returns -1 if no data available, 0 if the pipe
 * is closed, and the amount of data read otherwise.
 */
PCONSOLE_API int pconsole_read(pconsole_t *pconsole, void *buffer, int size);

/*
 * Write input to the Win32 console.  This input will be translated into
 * INPUT_RECORD objects.  (TODO: What about Ctrl-C and ESC?)
 */
PCONSOLE_API int pconsole_write(pconsole_t *pconsole,
				const void *buffer,
				int size);

/*
 * Change the size of the Windows console.
 */
PCONSOLE_API int pconsole_set_size(pconsole_t *pconsole, int cols, int rows);

/*
 * Gets the amount of data queued for output to the pconsole.  Use this API to
 * limit the size of the output buffer.
 */
PCONSOLE_API int pconsole_get_output_queue_size(pconsole_t *pconsole);

/*
 * Closes the pconsole.
 */
PCONSOLE_API void pconsole_close(pconsole_t *pconsole);

#ifdef __cplusplus
}
#endif

#endif /* PCONSOLE_H */
