======
winpty
======

winpty is a Windows software package that emulates a Unix pty-master interface
to Windows console programs.  It consists of a library and a tool for Cygwin
and MSYS for running a Windows console program under a Cygwin/MSYS pty, such as
that used by ``mintty`` or ``sshd``.

Prerequisites
=============

You need the following to build winpty:

* A Cygwin or MSYS installation
* GNU make
* A MinGW 32-bit g++ toolchain, v4 or later, to build ``winpty.dll`` and
  ``winpty-agent.exe``
* A g++ toolchain targeting Cygwin or MSYS, v3 or later, to build console.exe

MinGW appears to be split into two distributions -- MinGW (32-bit only) and
MinGW-w64 (which compiles both 32-bit and 64-bit binaries).  Either one is
acceptable, but the compiler must be v4 or later.

Cygwin packages
---------------

The default g++ compiler for Cygwin targets Cygwin itself, but Cygwin also
packages MinGW compilers from both the MinGW and MinGW-w64 projects.  As of
this writing, the necessary packages are:

* Either ``mingw-gcc-g++`` or ``mingw64-i686-gcc-g++`` (but not
  ``mingw64-x86_64-gcc-g++``)
* ``gcc4-g++``

Build
=====

In the project directory, run ``./configure``, then ``make``.

This will produce three binaries:

* ``build/winpty.dll``
* ``build/winpty-agent.exe``
* ``build/console.exe``

Using the Unix adapter
======================

To run a Windows console program in ``mintty`` or Cygwin ``sshd``, prepend 
``console.exe`` to the command-line::

    $ build/console.exe c:/Python27/python.exe
    Python 2.7.2 (default, Jun 12 2011, 15:08:59) [MSC v.1500 32 bit (Intel)] on win32
    Type "help", "copyright", "credits" or "license" for more information.
    >>> 10 + 20
    30
    >>> exit()
    $

How it works
============

The emulation starts a ``winpty-agent.exe`` process with a console window on a
hidden desktop.  It polls the console output for changes and converts them
to a stream of VT100 escape codes.  It converts input escape codes into console
input records.
