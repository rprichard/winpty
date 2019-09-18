# winpty

[![Build Status](https://ci.appveyor.com/api/projects/status/69tb9gylsph1ee1x/branch/master?svg=true)](https://ci.appveyor.com/project/rprichard/winpty/branch/master)

winpty is a Windows software package providing an interface similar to a Unix
pty-master for communicating with Windows console programs.  The package
consists of a library (libwinpty) and a tool for Cygwin and MSYS for running
Windows console programs in a Cygwin/MSYS pty.

The software works by starting the `winpty-agent.exe` process with a new,
hidden console window, which bridges between the console API and terminal
input/output escape codes.  It polls the hidden console's screen buffer for
changes and generates a corresponding stream of output.

The Unix adapter allows running Windows console programs (e.g. CMD, PowerShell,
IronPython, etc.) under `mintty` or Cygwin's `sshd` with
properly-functioning input (e.g. arrow and function keys) and output (e.g. line
buffering).  The library could be also useful for writing a non-Cygwin SSH
server.

## Supported Windows versions

winpty runs on Windows XP through Windows 10, including server versions.  It
can be compiled into either 32-bit or 64-bit binaries.

## Cygwin/MSYS adapter (`winpty.exe`)

### Prerequisites

You need the following to build winpty:

* A Cygwin or MSYS installation
* GNU make
* A MinGW g++ toolchain capable of compiling C++11 code to build `winpty.dll`
  and `winpty-agent.exe`
* A g++ toolchain targeting Cygwin or MSYS to build `winpty.exe`

Winpty requires two g++ toolchains as it is split into two parts. The
`winpty.dll` and `winpty-agent.exe` binaries interface with the native
Windows command prompt window so they are compiled with the native MinGW
toolchain.  The `winpty.exe` binary interfaces with the MSYS/Cygwin terminal so
it is compiled with the MSYS/Cygwin toolchain.

MinGW appears to be split into two distributions -- MinGW (creates 32-bit
binaries) and MinGW-w64 (creates both 32-bit and 64-bit binaries).  Either
one is generally acceptable.

#### Cygwin packages

The default g++ compiler for Cygwin targets Cygwin itself, but Cygwin also
packages MinGW-w64 compilers.  As of this writing, the necessary packages are:

* Either `mingw64-i686-gcc-g++` or `mingw64-x86_64-gcc-g++`.  Select the
  appropriate compiler for your CPU architecture.
* `gcc-g++`
* `make`

As of this writing (2016-01-23), only the MinGW-w64 compiler is acceptable.
The MinGW compiler (e.g. from the `mingw-gcc-g++` package) is no longer
maintained and is too buggy.

#### MSYS packages

For the original MSYS, use the `mingw-get` tool (MinGW Installation Manager),
and select at least these components:

* `mingw-developer-toolkit`
* `mingw32-base`
* `mingw32-gcc-g++`
* `msys-base`
* `msys-system-builder`

When running `./configure`, make sure that `mingw32-g++` is in your
`PATH`.  It will be in the `C:\MinGW\bin` directory.

#### MSYS2 packages

For MSYS2, use `pacman` and install at least these packages:

* `msys/gcc`
* `mingw32/mingw-w64-i686-gcc` or `mingw64/mingw-w64-x86_64-gcc`.  Select
  the appropriate compiler for your CPU architecture.
* `make`

MSYS2 provides three start menu shortcuts for starting MSYS2:

* MinGW-w64 Win32 Shell
* MinGW-w64 Win64 Shell
* MSYS2 Shell

To build winpty, use the MinGW-w64 {Win32,Win64} shortcut of the architecture
matching MSYS2.  These shortcuts will put the g++ compiler from the
`{mingw32,mingw64}/mingw-w64-{i686,x86_64}-gcc` packages into the `PATH`.

Alternatively, instead of installing `mingw32/mingw-w64-i686-gcc` or
`mingw64/mingw-w64-x86_64-gcc`, install the `mingw-w64-cross-gcc` and
`mingw-w64-cross-crt-git` packages.  These packages install cross-compilers
into `/opt/bin`, and then any of the three shortcuts will work.

### Building the Unix adapter

In the project directory, run `./configure`, then `make`, then `make install`.
By default, winpty is installed into `/usr/local`.  Pass `PREFIX=<path>` to
`make install` to override this default.

### Using the Unix adapter

To run a Windows console program in `mintty` or Cygwin `sshd`, prepend
`winpty` to the command-line:

    $ winpty powershell
    Windows PowerShell
    Copyright (C) 2009 Microsoft Corporation. All rights reserved.

    PS C:\rprichard\proj\winpty> 10 + 20
    30
    PS C:\rprichard\proj\winpty> exit

### Building the Cygwin adapter

In the project directory, run `./configure`, then `make`, then `make install`.
By default, winpty is installed into `/usr/local`.  Pass `PREFIX=<path>` to
`make install` to override this default.

### Using the Cygwin adapter inside Visual Studio Code

The integrated terminal in Visual Studio Code has numerous flaws[[11](#r1)] on Windows and winpty
versions lacking ConPTY support, which covers the lion's share of installations, as
ConPTY is only available on recent Windows 10 builds. Since winpty is ordinarily obliged to
interact with Cygwin processes' terminal via Windows' deficient console APIs, the Cygwin
adapter instead leverages Cygwin's native PTY implementation. This provides a direct
interface between `xterm.js` and Cygwin processes, addressing most of those flaws
and rendering the terminal usable.  
  
To obtain a fully functional integrated Cygwin terminal inside Visual Studio Code,
copy `/usr/local/bin/winpty-cygwin-agent.exe` (see above) as `winpty-agent.exe` into
the `Microsoft VS Code\resources\app\node_modules.asar.unpacked\node-pty\build\Release`
directory inside either `%ProgramFiles%` or `%ProgramFiles(x86)%` on 64-bit and
32-bit Windows respectively, preferably backing up the original `winpty-agent.exe`
image first.  
Ensure that the Cygwin installation is in `%Path%` either globally or for Visual
Studio Code.

``[1]`` <a href="https://github.com/microsoft/vscode/issues/45693" id="r1">Windows terminal issues caused by winpty · Issue #45693 · microsoft/vscode</a>

## Embedding winpty / MSVC compilation

See `src/include/winpty.h` for the prototypes of functions exported by
`winpty.dll`.

Only the `winpty.exe` binary uses Cygwin; all the other binaries work without
it and can be compiled with either MinGW or MSVC.  To compile using MSVC,
download gyp and run `gyp -I configurations.gypi` in the `src` subdirectory.
This will generate a `winpty.sln` and associated project files.  See the
`src/winpty.gyp` and `src/configurations.gypi` files for notes on dealing with
MSVC versions and different architectures.

Compiling winpty with MSVC currently requires MSVC 2013 or newer.

## Debugging winpty

winpty comes with a tool for collecting timestamped debugging output.  To use
it:

1. Run `winpty-debugserver.exe` on the same computer as winpty.
2. Set the `WINPTY_DEBUG` environment variable to `trace` for the
   `winpty.exe` process and/or the process using `libwinpty.dll`.

winpty also recognizes a `WINPTY_SHOW_CONSOLE` environment variable.  Set it
to 1 to prevent winpty from hiding the console window.

## Copyright

This project is distributed under the MIT license (see the `LICENSE` file in
the project root).

By submitting a pull request for this project, you agree to license your
contribution under the MIT license to this project.
