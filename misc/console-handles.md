Console Handles and Standard Handles
====================================

This document will attempt to explain how console handles work and how they
interact with process creation and console attachment and detachment.  It is
based on experiments that I ran against various versions of Windows from
Windows XP to Windows 10.




Common semantics
----------------

There are three flags to `CreateProcess` that affect what console a new console
process is attached to:

 - `CREATE_NEW_CONSOLE`
 - `CREATE_NO_WINDOW`
 - `DETACHED_PROCESS`

These flags are interpreted to produce what I will call the *CreationConsoleMode*.
`CREATE_NO_WINDOW` is ignored if combined with either other flag, and the
combination of `CREATE_NEW_CONSOLE` and `DETACHED_PROCESS` is an error:

| Criteria                                    | Resulting *CreationConsoleMode*       |
| ------------------------------------------- | ------------------------------------- |
| None of the flags (parent has a console)    | *Inherit*                             |
| None of the flags (parent has no console)   | *NewConsole*                          |
| `CREATE_NEW_CONSOLE`                        | *NewConsole*                          |
| `CREATE_NEW_CONSOLE | CREATE_NO_WINDOW`     | *NewConsole*                          |
| `CREATE_NO_WINDOW`                          | *NewConsoleNoWindow*                  |
| `DETACHED_PROCESS`                          | *Detach*                              |
| `DETACHED_PROCESS | CREATE_NO_WINDOW`       | *Detach*                              |
| `CREATE_NEW_CONSOLE | DETACHED_PROCESS`     | none - the `CreateProcess` call fails |
| All three flags                             | none - the `CreateProcess` call fails |

Windows' behavior depends on the *CreationConsoleMode*:

 * *NewConsole* or *NewConsoleNoWindow*:  Windows attaches the new process to
   a new console.  *NewConsoleNoWindow* is special--it creates an invisible
   console.  (Prior to Windows 7, `GetConsoleWindow` returned a handle to an
   invisible window.  Starting with Windows 7, `GetConsoleWindow` returns
   `NULL`.)

 * *Inherit*:  The child attaches to its parent's console.

 * *Detach*:  The child has no attached console, even if its parent had one.

I have not tested whether or how these flags affect non-console programs (i.e.
programs whose PE header subsystem is `WINDOWS` rather than `CONSOLE`).

There is one other `CreateProcess` flag that plays an important role in
understanding console handles -- `STARTF_USESTDHANDLES`.  This flag influences
whether the `AllocConsole` and `AttachConsole` APIs change the
"standard handles" (`STDIN/STDOUT/STDERR`) during the lifetime of the
new process, as well as the new process' initial standard handles, of course.
The standard handles are accessed with `GetStdHandle`
and `SetStdHandle`, which [are effectively wrappers around a global
`HANDLE[3]` variable](http://blogs.msdn.com/b/oldnewthing/archive/2013/03/07/10399690.aspx)
 -- these APIs do not use `DuplicateHandle` or `CloseHandle`
internally, and [while NT kernels objects are reference counted, `HANDLE`s
are not](http://blogs.msdn.com/b/oldnewthing/archive/2007/08/29/4620336.aspx).

The `FreeConsole` API detaches a process from its console, but it never alters
the standard handles.

(Note that by "standard handles", I am strictly referring to `HANDLE` values
and not `int` file descriptors or `FILE*` file streams provided by the C
language.  C and C++ standard I/O is implemented on top of Windows `HANDLE`s.)




Traditional semantics
---------------------

### Console handles and handle sets

In releases prior to Windows 8, console handles are not true NT handles.
Instead, the values are always multiples of four minus one (i.e. 0x3, 0x7,
0xb, 0xf, ...), and the functions in `kernel32.dll` detect the special handles
and perform LPCs to `csrss.exe` and/or `conhost.exe`.

A new console's initial console handles are always inheritable, but
non-inheritable handles can also be created.  The inheritability can usually
be changed, except on Windows 7 (see notes below).

Traditional console handles cannot be duplicated to other processes.  If such
a handle is used with `DuplicateHandle`, the source and target process handles
must be the `GetCurrentProcess()` pseudo-handle, not a real handle to the
current process.

Whenever a process creates a new console (either during startup or when it
calls `AllocConsole`), Windows replaces that process' set of open
console handles (its *ConsoleHandleSet*) with three inheritable handles
(0x3, 0x7, 0xb).  Whenever a process attaches to an existing console (either
during startup or when it calls `AttachConsole`), Windows completely replaces
that process' *ConsoleHandleSet* with the set of inheritable open handles
from the originating process.  These "imported" handles are also inheritable.

### Standard handles, CreateProcess

The manner in which Windows sets standard handles is influenced by two flags:

 - Whether `STARTF_USESTDHANDLES` was set in `STARTUPINFO` when the process
   started (*UseStdHandles*)
 - Whether the `CreateProcess` parameter, `bInheritHandles`, was `TRUE`
   (*InheritHandles*)

From Window XP up until Windows 8, `CreateProcess` sets standard handles as
follows:

 - Regardless of *ConsoleCreationMode*, if *UseStdHandles*, then handles are
   set according to `STARTUPINFO`.  Windows makes no attempt to validate the
   handle, nor will it treat a non-inheritable handle as inheritable simply
   because it is listed in `STARTUPINFO`.

   Otherwise, find the next applicable rule.

 - If *ConsoleCreationMode* is *NewConsole* or *NewConsoleNoWindow*, then
   Windows sets the handles to (0x3, 0x7, 0xb).

 - If *ConsoleCreationMode* is *Inherit*:

    - If *InheritHandles*, then the parent's standard
      handles are copied as-is to the child, without exception.

    - If !*InheritHandles*, then Windows duplicates each
      of the parent's non-console standard handles into the child.  Any
      standard handle that looks like a traditional console handle, up to
      0x0FFFFFFF, is copied as-is, whether or not the handle is open.
      <sup>[[1]](#foot_inv_con)</sup>

      If Windows fails to duplicate a handle for any reason (e.g. because
      it is `NULL` or not open), then the child's new handle is `NULL`.
      If the parent's handle is the current process psuedo-handle, then
      the child's handle is a non-pseudo non-inheritable handle to the
      parent process.  The child handles have the same inheritability as
      the parent handles.  These handles are not closed by `FreeConsole`.

The `bInheritHandles` parameter to `CreateProcess` does not affect whether
console handles are inherited.  Console handles are inherited if and only if
they are marked inheritable.  The `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`
attribute added in Vista does not restrict console handle inheritance, and
erratic behavior may result from specifying a traditional console handle in
`PROC_THREAD_ATTRIBUTE_HANDLE_LIST`'s `HANDLE` list.  (See the
`Test_CreateProcess_STARTUPINFOEX` test in `misc/buffer-tests`.)

### AllocConsole, AttachConsole

`AllocConsole` and `AttachConsole` set the standard handles as follows:

 - If *UseStdHandles*, then Windows does not modify the standard handles.
 - If !*UseStdHandles*, then Windows changes the standard handles to
   (0x3, 0x7, 0xb), even if those handles are not open.

### FreeConsole

After calling `FreeConsole`, no console APIs work, and all previous console
handles are apparently closed -- even `GetHandleInformation` fails on the
handles.  `FreeConsole` has no effect on the `STDIN/STDOUT/STDERR` values.




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

Unlike traditional console handles, modern console handles **can** be
duplicated to other processes.

### Standard handles, CreateProcess

Whenever a process is attached to a console (during startup, `AttachConsole`,
or `AllocConsole`), Windows will sometimes create new *Unbound* console
objects and assign them to one or more standard handles.  If it assigns
to both `STDOUT` and `STDERR`, it reuses the same new *Unbound*
*Output* object for both.  If `FreeConsole` is called, it will close these
console handles.

As with previous releases, standard handle determination is affected by the
*UseStdHandles* and *InheritHandles* flags.

(N.B.: The combination of !*InheritHandles* and *UseStdHandles* does not
really make sense, so it's not surprising to see Windows 8 ignore
*UseStdHandles* in this case, as it sometimes does.)

Starting in Windows 8, `CreateProcess` sets standard handles as follows:

 - Regardless of *ConsoleCreationMode*, if *InheritHandles* and
   *UseStdHandles*, then handles are set according to `STARTUPINFO`,
   except that each `NULL` handle is replaced with a new console
   handle.  As with previous releases, Windows makes no effort to validate
   the handle, nor will it treat a non-inheritable handle as inheritable
   simply because it is listed in `STARTUPINFO`.

   Otherwise, find the next applicable rule.

 - If *ConsoleCreationMode* is *NewConsole* or *NewConsoleNoWindow*, then
   Windows sets all three handles to new console handles.

 - If *ConsoleCreationMode* is *Inherit*:

    - If *InheritHandles* and !*UseStdHandles*, then the parent's standard
      handles are copied as-is to the child, without exception.

    - If !*InheritHandles* and *UseStdHandles*, then all three handles become
      `NULL`.

    - If !*InheritHandles* and !*UseStdHandles*, then Windows duplicates each
      parent standard handle into the child.

      As with previous releases, the current process pseudo-handle becomes a
      true process handle to the parent.  However, starting with Windows 8.1,
      it instead translates to NULL.  (i.e. The bug was fixed.)  `FreeConsole`
      in Windows 8 does not close these duplicated handles, in general,
      because they're not necessarily console handles.  Even if one does
      happen to be a console handle, `FreeConsole` still does not close it.
      (That said, these handles are likely to be console handles.)

 - If *ConsoleCreationMode* is *Detach*:

    - XXX: ...

XXX: Also, I don't expect the `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` attribute
to matter here, but it's needs to be tested.

### AllocConsole, AttachConsole

`AllocConsole` and `AttachConsole` set the standard handles as follows:

 - If *UseStdHandles*, then Windows opens a console handle for each standard
   handle that is currently `NULL`.

 - If !*UseStdHandles*, then Windows opens three new console handles.

### Implicit screen buffer refcount

When a process' console state is initialized (at startup, `AllocConsole`
or `AttachConsole`), Windows increments a refcount on the console's
currently active screen buffer, which decrements only when the process
detaches from the console.  All *Unbound* *Output* console objects reference
this screen buffer.

### FreeConsole

As in previous Windows releases, `FreeConsole` in Windows 8 does not change
the `STDIN/STDOUT/STDERR` values.  If Windows opened new console handles for
`STDIN/STDOUT/STDERR` when it initialized the process' console state, then
`FreeConsole` will close those handles.  Otherwise, `FreeConsole` will only
close the two internal handles.

### Interesting properties

 * `FreeConsole` can close a non-console handle.  This happens if:

     1. Windows had opened handles during console initialization.
     2. The program closes its standard handles and opens new non-console
        handles with the same values.
     3. The program calls `FreeConsole`.

   (Perhaps programs are not expected to close their standard handles.)

 * Console handles--*Bound* or *Unbound*--can be duplicated to other
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
   three more for itself.  All of the handles would be usable (if the
   program kept track of them somehow).

Other notes
-----------

### SetActiveConsoleScreenBuffer

Screen buffers are referenced counted.  Changing the active screen buffer
with `SetActiveConsoleScreenBuffer` does not increment a refcount on the
buffer.  If the active buffer's refcount hits zero, then Windows chooses
another buffer and activates it.

### `CREATE_NO_WINDOW` process creation flag

The documentation for `CREATE_NO_WINDOW` is confusing:

>  The process is a console application that is being run without a
>  console window. Therefore, the console handle for the application is
>  not set.
>
>  This flag is ignored if the application is not a console application,
>  or if it is used with either `CREATE_NEW_CONSOLE` or `DETACHED_PROCESS`.

Here's what's evident from examining the OS behavior:

 * Specifying both `CREATE_NEW_CONSOLE` and `DETACHED_PROCESS` causes the
   `CreateProcess` call to fail.

 * If `CREATE_NO_WINDOW` is specified together with `CREATE_NEW_CONSOLE` or
   `DETACHED_PROCESS`, it is quietly ignored, just as documented.

 * Otherwise, `CreateProcess` behaves the same way with `CREATE_NO_WINDOW` as
   it does with `CREATE_NEW_CONSOLE`, except that the new console either has
   a hidden window (before Windows 7) or has no window at all (Windows 7
   and later).  These situations can be distinguished using the
   `GetConsoleWindow` and `IsWindowVisible` calls.  `GetConsoleWindow` returns
   `NULL` starting with Windows 7.

### Windows XP pipe read handle inheritance anomaly

On Windows XP, `CreateProcess` fails to propagate a handle in this situation:

 - `bInheritHandles` is `FALSE`.
 - `STARTF_USESTDHANDLES` is not specified in `STARTUPINFO.dwFlags`.
 - One of the `STDIN/STDOUT/STDERR` handles is set to the read end of an
   anonymous pipe.

In this situation, Windows XP will set the child process's standard handle to
`NULL`.  The write end of the pipe works fine.  Passing a `bInheritHandles`
of `TRUE` (and an inheritable pipe handle) works fine.  Using
`STARTF_USESTDHANDLES` also works.  See `Test_CreateProcess_DefaultInherit`
in `misc/buffer-tests` for a test case.

### Windows Vista BSOD

It is easy to cause a BSOD on Vista and Server 2008 by (1) closing all handles
to the last screen buffer, then (2) creating a new screen buffer:

    #include <windows.h>
    int main() {
        FreeConsole();
        AllocConsole();
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




Footnotes
---------

<a name="foot_inv_con">1</a>: From the previous discussion, it follows that
if a standard handle is a non-inheritable console handle, then the child's
standard handle will be invalid:

 - Traditional console standard handles are copied as-is to the child.
 - The child has the same *ConsoleHandleSet* as the parent, excluding
   non-inheritable handles.

It's an interesting edge case, though, so I test for it specifically.
