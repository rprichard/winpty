#!python3
import os
import sys
sys.path.insert(1, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
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

    check_call([
        abspath('{}.exe'.format(setup)),
        '-l', abspath('{}-packages'.format(cygwin)),
        '-P', 'gcc-g++,make',
        '-s', 'http://mirrors.kernel.org/sourceware/cygwin',
        '-R', abspath(cygwin),
        '--no-admin', '--no-desktop', '--no-shortcuts', '--no-startmenu', '--quiet-mode',
    ])

    check_call(['{}/bin/ash.exe'.format(cygwin), '/bin/rebaseall', '-v'])

    cygVer = dllversion.fileVersion('{}/bin/cygwin1.dll'.format(cygwin))
    gppVer = getGppVer('{}/bin/g++.exe'.format(cygwin))

    filename = '{}\\{}-{}-dll{}-gcc{}.7z'.format(artifactDir, cygwin, buildTimeStamp, cygVer, gppVer)
    rmpath(filename)

    open(cygwin + '/tmp/.keep', 'wb').close()

    check_call(['7z', 'a', '-mx=9', filename] + glob_paths([
        cygwin + '/dev',
        cygwin + '/etc/setup',
        cygwin + '/tmp/.keep',
        cygwin + '/bin',
        cygwin + '/lib',
        cygwin + '/usr/include',
        cygwin + '/usr/*-pc-cygwin',
    ]))
