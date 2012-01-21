#!python -u
# Run with native CPython.  Needs pywin32 extensions.

import win32pipe
import win32api
import win32file
import time
import threading

# A message may not be larger than this size.
MSG_SIZE=4096

serverPipe = win32pipe.CreateNamedPipe(
    "\\\\.\\pipe\\DebugServer",
    win32pipe.PIPE_ACCESS_DUPLEX,
    win32pipe.PIPE_TYPE_MESSAGE | win32pipe.PIPE_READMODE_MESSAGE,
    win32pipe.PIPE_UNLIMITED_INSTANCES,
    MSG_SIZE,
    MSG_SIZE,
    10 * 1000,
    None)
while True:
    win32pipe.ConnectNamedPipe(serverPipe, None)
    (ret, data) = win32file.ReadFile(serverPipe, MSG_SIZE)
    print(data.decode())

    # The client uses CallNamedPipe to send its message.  CallNamedPipe waits
    # for a reply message.  If I send a reply, however, using WriteFile, then
    # sometimes WriteFile fails with:
    #     pywintypes.error: (232, 'WriteFile', 'The pipe is being closed.')
    # I can't figure out how to write a strictly correct pipe server, but if
    # I comment out the WriteFile line, then everything seems to work.  I
    # think the DisconnectNamedPipe call aborts the client's CallNamedPipe
    # call normally.

    try:
        win32file.WriteFile(serverPipe, b'OK')
    except:
        pass
    win32pipe.DisconnectNamedPipe(serverPipe)
