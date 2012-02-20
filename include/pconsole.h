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

/*
 * pconsole API.
 */

/*
 * Starts a new pconsole instance with the given size.
 */
PCONSOLE_API pconsole_t *pconsole_open(int cols, int rows);

/*
 * Start a child process.  Either (but not both) of appname and cmdline may
 * be NULL.  cwd and env may be NULL.  env is a pointer to an environment
 * block like that passed to CreateProcess.
 *
 * This function never modifies the cmdline, unlike CreateProcess.
 *
 * Only one child process may be started.  After the child process exits, the
 * agent will flush and close the data pipe.
 */
PCONSOLE_API int pconsole_start_process(pconsole_t *pc,
					const wchar_t *appname,
					const wchar_t *cmdline,
					const wchar_t *cwd,
					const wchar_t *env);

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
