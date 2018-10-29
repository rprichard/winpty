#!python3
import os
import sys
import util

import re
import shutil
import subprocess

import dllversion

from os.path import abspath
from subprocess import check_call
from util import glob_paths, rmpath, mkdirs, buildTimeStamp, projectDir, getGppVer


sys.platform == 'win32' or sys.exit('error: script only runs on Windows (no Cygwin/MSYS)')
shutil.which('7z')      or sys.exit('error: 7z missing')
shutil.which('curl')    or sys.exit('error: curl missing')


buildDir = os.path.join(projectDir, 'out\\build-cygwin')
artifactDir = os.path.join(projectDir, 'out\\artifact')
rmpath(buildDir)
mkdirs(buildDir)
mkdirs(artifactDir)

os.chdir(buildDir)

for setup, cygwin in (('setup-x86_64', 'cygwin64'), ('setup-x86', 'cygwin32')):

    check_call(['curl', '-fL', '-O', 'https://cygwin.com/{}.exe'.format(setup)])

    # Include cross-compilers targeting both architectures, in both packages,
    # because I might want to require having both cross-compilers in the build
    # system at some point. (e.g. if winpty includes a synchronous WinEvents
    # hook loaded into conhost.exe)
    cygwinPackages = [
        'gcc-g++',
        'make',
        'mingw64-i686-gcc-g++',
        'mingw64-x86_64-gcc-g++',
    ]
    check_call([
        abspath('{}.exe'.format(setup)),
        '-l', abspath('{}-packages'.format(cygwin)),
        '-P', ','.join(cygwinPackages),
        '-s', 'http://mirrors.kernel.org/sourceware/cygwin',
        '-R', abspath(cygwin),
        '--no-admin', '--no-desktop', '--no-shortcuts', '--no-startmenu', '--quiet-mode',
    ])

    check_call(['{}/bin/ash.exe'.format(cygwin), '/bin/rebaseall', '-v'])

    dllVer = dllversion.fileVersion('{}/bin/cygwin1.dll'.format(cygwin))
    cygGccVer = getGppVer(['{}/bin/g++.exe'.format(cygwin), '--version'])
    winGccVer = getGppVer(['{}/bin/x86_64-w64-mingw32-g++.exe'.format(cygwin), '--version'])
    filename = '{}\\{}-{}-dll{}-cyggcc{}-wingcc{}.7z'.format(
        artifactDir, cygwin, buildTimeStamp, dllVer, cygGccVer, winGccVer)
    rmpath(filename)

    open(cygwin + '/tmp/.keep', 'wb').close()

    check_call(['7z', 'a', '-mx=9', filename] + glob_paths([
        cygwin + '/dev',
        cygwin + '/etc/setup',
        cygwin + '/tmp/.keep',
        cygwin + '/bin',
        cygwin + '/lib',
        cygwin + '/usr/include',
        cygwin + '/usr/share/doc',
        cygwin + '/usr/share/info',
        cygwin + '/usr/share/man',
        cygwin + '/usr/*-pc-cygwin',
        cygwin + '/usr/*-w64-mingw32',
    ]))
