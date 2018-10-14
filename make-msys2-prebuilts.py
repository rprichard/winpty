#!python3
import os
import sys
import util

import hashlib
import re
import shutil
import subprocess
import urllib

import dllversion

from os.path import abspath
from subprocess import check_call
from util import glob_paths, rmpath, mkdirs, buildTimeStamp, projectDir, getGppVer


def sha256(path):
    with open(path, 'rb') as fp:
        return hashlib.sha256(fp.read()).hexdigest()

def checkSha256(path, expected):
    actual = sha256(path)
    if actual != expected:
        sys.exit('error: sha256 hash mismatch on {}: expected {}, found {}'.format(
            path, expected, actual))


sys.platform == 'win32' or sys.exit('error: script only runs on Windows (no Cygwin/MSYS)')
shutil.which('7z')      or sys.exit('error: 7z missing')
shutil.which('curl')    or sys.exit('error: curl missing')

buildDir = os.path.join(projectDir, 'out\\build-msys2')
artifactDir = os.path.join(projectDir, 'out\\artifact')
rmpath(buildDir)
mkdirs(buildDir)
mkdirs(artifactDir)

os.chdir(buildDir)

check_call(['curl', '-fL', '-O', 'http://repo.msys2.org/distrib/i686/msys2-base-i686-20180531.tar.xz'])
check_call(['curl', '-fL', '-O', 'http://repo.msys2.org/distrib/x86_64/msys2-base-x86_64-20180531.tar.xz'])
checkSha256('msys2-base-i686-20180531.tar.xz',   '8ef5b18c4c91f3f2394823f1981babdee78a945836b2625f091ec934b1a37d32')
checkSha256('msys2-base-x86_64-20180531.tar.xz', '4e799b5c3efcf9efcb84923656b7bcff16f75a666911abd6620ea8e5e1e9870c')

for name, arch in (('msys64', 'x86_64'), ('msys32', 'i686')):

    baseArchive = 'msys2-base-{}-20180531'.format(arch)
    check_call(['7z', 'x', '{}.tar.xz'.format(baseArchive)])
    check_call(['7z', 'x', '{}.tar'.format(baseArchive)])

    bashPath = abspath(name + '\\usr\\bin\\bash.exe')

    check_call([bashPath, '--login', '-c', 'exit'])
    good = False
    for i in range(5):
        # Apparently in the base install, the 'msys-runtime' and 'catgets'
        # packages are incompatible, and passing --ask=20 confirms to MSYS2
        # that we should do the necessary thing (remove catgets, I guess?)
        # See https://github.com/Alexpux/MSYS2-packages/issues/1141.
        cmd = [bashPath, '--login', '-c', 'pacman --ask=20 --noconfirm -Syuu']
        print('Running {} ...'.format(repr(cmd)))
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding='utf-8')
        sys.stdout.write(p.stdout)
        if p.returncode != 0:
            sys.exit('error: MSYS2 system update failed')

        good = 'there is nothing to do' in [x.strip() for x in p.stdout.splitlines()]

    good or sys.exit('error: MSYS2 system update never finished')

    # Include cross-compilers targeting both architectures, in both packages,
    # because I might want to require having both cross-compilers in the build
    # system at some point. (e.g. if winpty includes a synchronous WinEvents
    # hook loaded into conhost.exe)
    msysPackages = [
        'msys/gcc',
        'msys/make',
        'msys/tar',
        'mingw-w64-cross-toolchain',
    ]
    cmd = [bashPath, '--login', '-c', 'pacman --noconfirm -S ' + ' '.join(msysPackages)]
    print('Running {} ...'.format(repr(cmd)))
    check_call(cmd)

    # pacman/gpg start gpg-agent and scdaemon processes, which prevents
    # rebasing and would break appveyor. Kill pacman's gpg-agent.
    # https://help.appveyor.com/discussions/problems/16396-build-freezes-between-commands-in-appveyoryml
    check_call([
        '{}/usr/bin/gpgconf.exe'.format(name),
        '--homedir', '/etc/pacman.d/gnupg',
        '--kill', 'all',
    ])

    # The -p option passed by autorebase.bat doesn't look necessary. It relaxes
    # the sanity checking to allow more than just ash.exe/dash.exe processes.
    check_call(['{}/usr/bin/ash.exe'.format(name), '/usr/bin/rebaseall', '-v'])

    dllVer = dllversion.fileVersion('{}/usr/bin/msys-2.0.dll'.format(name))
    msysGccVer = getGppVer('{}/usr/bin/g++.exe'.format(name))
    winGccVer = getGppVer('{}/mingw64/bin/x86_64-w64-mingw32-g++.exe'.format(name))
    filename = '{}\\{}-{}-dll{}-msysgcc{}-wingcc{}.7z'.format(
        artifactDir, name, buildTimeStamp, dllVer, msysGccVer, winGccVer)
    rmpath(filename)

    open(name + '/tmp/.keep', 'wb').close()
    open(name + '/etc/.keep', 'wb').close()

    check_call(['7z', 'a', '-mx=9', filename] + glob_paths([
        name + '/autorebase.bat',
        name + '/dev',
        name + '/etc/.keep',
        name + '/tmp/.keep',
        name + '/usr/bin',
        name + '/usr/lib',
        name + '/usr/include',
        name + '/usr/*-pc-msys',
        name + '/opt',
    ]))
