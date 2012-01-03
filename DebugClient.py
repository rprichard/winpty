#!/usr/bin/env python
# Run with native CPython.  Needs pywin32 extensions.

import winerror
import win32pipe
import win32file
import win32api
import sys
import pywintypes
import time

if len(sys.argv) != 2:
    print("Usage: %s message" % sys.argv[0])
    sys.exit(1)

message = "[%05.3f %s]: %s" % (time.time() % 100000, sys.argv[0], sys.argv[1])

win32pipe.CallNamedPipe(
    "\\\\.\\pipe\\DebugServer",
    message.encode(),
    16,
    win32pipe.NMPWAIT_WAIT_FOREVER)
