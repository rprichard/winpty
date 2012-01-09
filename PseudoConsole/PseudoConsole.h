#ifndef AGENTCLIENT_H
#define AGENTCLIENT_H

#include <stdlib.h>

#ifdef PSEUDOCONSOLE
#define PSEUDOCONSOLE_DLLEXPORT __declspec(dllexport)
#else
#define PSEUDOCONSOLE_DLLEXPORT __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct Console;

PSEUDOCONSOLE_DLLEXPORT
Console *consoleOpen(int cols, int rows);

PSEUDOCONSOLE_DLLEXPORT
int consoleStartShell(Console *console,
                      const wchar_t *program,
                      const wchar_t *cmdline,
                      const wchar_t *cwd);

PSEUDOCONSOLE_DLLEXPORT
void consoleCancelIo(Console *console);

PSEUDOCONSOLE_DLLEXPORT
int consoleRead(Console *console, void *buffer, int size);

PSEUDOCONSOLE_DLLEXPORT
int consoleWrite(Console *console, const void *buffer, int size);

PSEUDOCONSOLE_DLLEXPORT
void consoleFree(Console *console);

#ifdef __cplusplus
}
#endif

#endif // AGENTCLIENT_H
