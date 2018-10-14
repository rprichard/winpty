#!python3
import os
import sys
sys.path.insert(1, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
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

check_call(['curl', '-fL', '-O', 'http://repo.msys2.org/distrib/i686/msys2-base-i686-20161025.tar.xz'])
check_call(['curl', '-fL', '-O', 'http://repo.msys2.org/distrib/x86_64/msys2-base-x86_64-20161025.tar.xz'])
checkSha256('msys2-base-i686-20161025.tar.xz',   '8bafd3d52f5a51528a8671c1cae5591b36086d6ea5b1e76e17e390965cf6768f')
checkSha256('msys2-base-x86_64-20161025.tar.xz', 'bb1f1a0b35b3d96bf9c15092da8ce969a84a134f7b08811292fbc9d84d48c65d')

for name, arch in (('msys64', 'x86_64'), ('msys32', 'i686')):

    baseArchive = 'msys2-base-{}-20161025'.format(arch)
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

    cmd = [bashPath, '--login', '-c', 'pacman --noconfirm -S msys/gcc msys/make msys/tar']
    print('Running {} ...'.format(repr(cmd)))
    check_call(cmd)

    # The -p option passed by autorebase.bat doesn't look necessary. It relaxes
    # the sanity checking to allow more than just ash.exe/dash.exe processes.
    check_call(['{}/usr/bin/ash.exe'.format(name), '/usr/bin/rebaseall', '-v'])

    msysVer = dllversion.fileVersion('{}/usr/bin/msys-2.0.dll'.format(name))
    gppVer = getGppVer('{}/usr/bin/g++.exe'.format(name))

    filename = '{}\\{}-{}-dll{}-gcc{}.7z'.format(artifactDir, name, buildTimeStamp, msysVer, gppVer)
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
    ]))
