Console Handles
===============

This document will attempt to explain how console handles work and how they
interact with process creation and console attachment and detachment.  It is
based on experiments that I ran against various versions of Windows.

Traditional semantics
---------------------

In releases prior to Windows 8, console handles are not true NT handles.
Rather, the values are always multiples of four minus one (i.e. 0x3, 0x7,
0xb, 0xf, ...), and the functions in `kernel32.dll` detect the special handles
and perform RPCs to `csrss.exe` and/or `conhost.exe`.

Whenever a new console is created, Windows replaces the attached process'
set of open console handles (*ConsoleHandleSet*) with three inheritable handles
(0x3, 0x7, 0xb) and sets `STDIN/STDOUT/STDERR` to these handles.  This
behavior applies in these cases:

 - at process startup if `CreateProcess` was called with `CREATE_NEW_CONSOLE` set
 - at process startup if `CreateProcess` was called with `CREATE_NO_WINDOW` set
 - when `AllocConsole` is called


XXX: `AllocConsole` seemed to leave `STDIN/STDOUT/STDERR` alone on the BSOD Vista
test case, when launched from Cygwin.  In that case, the standard handles were already
pointing at the Cygwin pty pipes.  Do more testing here...  Also test AttachConsole
with pipe stanard handles.


Whenever a process inherits or attaches to an existing console, its
*ConsoleHandleSet* is completely replaced by the set of inheritable open
handles from the originating process.  Additionally, `AttachConsole` resets
the `STDIN/STDOUT/STDERR` handles to (0x3, 0x7, 0xb), even if those handles
are not open.  This behavior applies in these cases:

 - at process startup (all other cases)
 - when `AttachConsole` is called

After calling `FreeConsole`, no console APIs work, and all previous console
handles are apparently closed -- even `GetHandleInformation` fails on the
handles.  `FreeConsole` has no effect on the `STDIN/STDOUT/STDERR` values.

A new console's initial console handles are always inheritable, but
non-inheritable handles can also be created.  The inheritability can usually
be changed, except on Windows 7 (see notes below).  The `bInheritHandles`
parameter to `CreateProcess` has no effect on console handles, which are
always inherited if they are marked inheritable.  (As such, the
`PROC_THREAD_ATTRIBUTE_HANDLE_LIST` attribute should be irrelevant to console
handles; they would already be inherited.)  (XXX: However, verify that the
attribute does not suppress inheritance.)

Traditional console handles cannot be duplicated to other processes.

Windows 8 semantics
-------------------

### Console handles

Starting with Windows 8, console handles are true NT kernel handles that
reference NT kernel objects.

If a process is attached to a console, then it will have two handles open
to `\Device\ConDrv` that Windows uses internally.  These handles are never
observable by the user program.  (To view them, use `handle.exe` from
sysinternals, i.e. `handle.exe -a -p <pid>`.)  A process with no attached
console never has these two handles open.

Ordinary I/O console handles are also associated with `\Device\ConDrv`.  The
underlying console objects can be classified in two ways:

 - *Input* vs *Output*
 - *Bound* vs *Unbound*

A *Bound* *Input* object is tied to a particular console, and a *Bound*
*Output* object is tied to a particular console screen buffer.  These
objects are usable only if the process is attached to the correct
console.  *Bound* objects are created through these methods only:

 - `CreateConsoleScreenBuffer`
 - opening `CONIN$` or `CONOUT$`

Most console objects are *Unbound*, which are created during console
initialization.  For any given console API call, an *Unbound* *Input* object
refers to the currently attached console's input queue, and an *Unbound*
*Output* object refers to the screen buffer that was active during the calling
process' console initialization.  These objects are usable as long as the
calling process has any console attached.

### Console initialization

When a process' console state is initialized, Windows may open new handles.
This happens in these instances:

 - at process startup if `CreateProcess` was called with `CREATE_NEW_CONSOLE` set
 - at process startup if `CreateProcess` was called with `CREATE_NO_WINDOW` set
 - at process startup if `CreateProcess` was called with `bInheritHandles=FALSE`
 - when `AttachConsole` is called
 - when `AllocConsole` is called

When it opens handles, Windows sets `STDIN/STDOUT/STDERR` to three newly opened
handles to two *Unbound* console objects.

Regardless of whether new handles were opened, Windows increments a refcount
on the active screen buffer, which decrements only when the process detaches
from the console.

As in previous Windows releases, `FreeConsole` in Windows 8 does not change
the `STDIN/STDOUT/STDERR` values.  If Windows opened new handles for
`STDIN/STDOUT/STDERR` when it initialized the process' console state, then
`FreeConsole` will close those handles.  Otherwise, `FreeConsole` will only
close the two internal handles.

### Interesting Consequences

 * `FreeConsole` can close a non-console handle.  This happens if:

     1. Windows had opened handles during console initialization.
     2. The program closes its standard handles and opens new non-console
        handles with the same values.
     3. The program calls `FreeConsole`.

   (Perhaps programs are not expected to close their standard handles.)

 * Console handles--*Bound* or *Unbound*--**can** be duplicated to other
   processes.  The duplicated handles are sometimes usable, especially
   if *Unbound*.  The same *Unbound* *Output* object can be open in two
   different processes and refer to different screen buffers in the same
   console or in different consoles.

 * Even without duplicating console handles, it is possible to have open
   console handles that are not usable, even with a console attached.

 * Dangling *Bound* handles are not allowed, so it is possible to have
   consoles with no attached processes.  The console cannot be directly
   modified (or attached to), but its visible content can be changed by
   closing *Bound* *Output* handles to activate other screen buffers.

 * A program that repeatedly reinvoked itself with `CREATE_NEW_CONSOLE` and
   `bInheritHandles=TRUE` would accumulate console handles.  Each child
   would inherit all of the previous child's console handles, then allocate
   three more for itself.  All of the handles would be usable if the
   program kept track of them somehow.

Other notes
-----------

### SetActiveConsoleScreenBuffer

Screen buffers are referenced counted.  Changing the active screen buffer
with `SetActiveConsoleScreenBuffer` does not increment a refcount on the
buffer.  If the active buffer's refcount hits zero, then Windows chooses
another buffer and activates it.

### Windows Vista BSOD

It is easy to cause a BSOD on Vista and Server 2008 by (1) closing all handles
to the last screen buffer, then (2) creating a new screen buffer:

    #include <windows.h>
    int main() {
        FreeConsole();
        AllocConsole();
        CloseHandle((HANDLE)0x3);
        CloseHandle((HANDLE)0x7);
        CloseHandle((HANDLE)0xb);
        CreateConsoleScreenBuffer(
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CONSOLE_TEXTMODE_BUFFER,
            NULL);
        return 0;
    }

### Windows 7 inheritability

 * Calling `DuplicateHandle(h, FALSE)` on an inheritable console handle
   produces an inheritable handle.  According to documentation and previous
   releases, it should be non-inheritable.

 * Calling `SetHandleInformation` fails on console handles.

### Windows 7 `conhost.exe` crash with `CONOUT$`

XXX: Document this.  It's a problem...
