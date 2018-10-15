#!python

# Copyright (c) 2015 Ryan Prichard
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

#
# Run with native CPython 2.7 on a 64-bit computer.
#

import common_ship

import multiprocessing
import os
import shutil
import subprocess
import sys


def dllVersion(path):
    version = subprocess.check_output(
        ["powershell.exe",
        "[System.Diagnostics.FileVersionInfo]::GetVersionInfo(\"" + path + "\").FileVersion"])
    return version.strip()


os.chdir(common_ship.topDir)

URL_BASE = "file://c:/rprichard/proj/winpty-cygwin-prebuilts/out/artifact/"

# Determine other build parameters.
print("Determining Cygwin/MSYS2 DLL versions...")
sys.stdout.flush()
BUILD_TARGETS = [
    {
        "systemName": "msys32",
        "systemArchive":               "msys32-20181014-dll2.11.1-msysgcc7.3.0-wingcc7.3.0.7z",
        "systemArchiveUrl": URL_BASE + "msys32-20181014-dll2.11.1-msysgcc7.3.0-wingcc7.3.0.7z",
        "packageName": "msys2-{dllver}-ia32",
        "rebase": ["usr\\bin\\ash.exe", "/usr/bin/rebaseall", "-v"],
        "dll": "usr\\bin\\msys-2.0.dll",
        "path": ["opt\\bin", "usr\\bin"],
    },
    {
        "systemName": "msys64",
        "systemArchive":               "msys64-20181014-dll2.11.1-msysgcc7.3.0-wingcc7.3.0.7z",
        "systemArchiveUrl": URL_BASE + "msys64-20181014-dll2.11.1-msysgcc7.3.0-wingcc7.3.0.7z",
        "packageName": "msys2-{dllver}-x64",
        "rebase": ["usr\\bin\\ash.exe", "/usr/bin/rebaseall", "-v"],
        "dll": "usr\\bin\\msys-2.0.dll",
        "path": ["opt\\bin", "usr\\bin"],
    },
    {
        "systemName": "cygwin32",
        "systemArchive":               "cygwin32-20181014-dll2.11.1-cyggcc7.3.0-wingcc6.4.0.7z",
        "systemArchiveUrl": URL_BASE + "cygwin32-20181014-dll2.11.1-cyggcc7.3.0-wingcc6.4.0.7z",
        "packageName": "cygwin-{dllver}-ia32",
        "rebase": ["bin\\ash.exe", "/bin/rebaseall", "-v"],
        "dll": "bin\\cygwin1.dll",
        "path": ["bin"],
    },
    {
        "systemName": "cygwin64",
        "systemArchive":               "cygwin64-20181014-dll2.11.1-cyggcc7.3.0-wingcc6.4.0.7z",
        "systemArchiveUrl": URL_BASE + "cygwin64-20181014-dll2.11.1-cyggcc7.3.0-wingcc6.4.0.7z",
        "packageName": "cygwin-{dllver}-x64",
        "rebase": ["bin\\ash.exe", "/bin/rebaseall", "-v"],
        "dll": "bin\\cygwin1.dll",
        "path": ["bin"],
    },
]


def targetSystemDir(target):
    return os.path.abspath(os.path.join("ship", "tmp", target["systemName"]))


def setup_cyg_system(target):
    common_ship.mkdir("ship/tmp")

    systemDir = targetSystemDir(target)
    systemArchivePath = os.path.abspath(os.path.join("ship", "tmp", target["systemArchive"]))
    binPaths = [os.path.join(systemDir, p) for p in target["path"]]
    binPaths += common_ship.defaultPathEnviron.split(";")
    newPath = ";".join(binPaths)

    if not os.path.exists(systemDir):
        if not os.path.exists(systemArchivePath):
            subprocess.check_call(["curl.exe", "-fL", "-o", systemArchivePath, target["systemArchiveUrl"]])
            assert os.path.exists(systemArchivePath)
        subprocess.check_call(["7z.exe", "x", systemArchivePath], cwd=os.path.dirname(systemDir))
        with common_ship.ModifyEnv(PATH=newPath):
            rebaseCmd = target["rebase"]
            rebaseCmd[0] = os.path.join(systemDir, rebaseCmd[0])
            subprocess.check_call(rebaseCmd)
    assert os.path.exists(systemDir)

    return newPath


def buildTarget(target):
    newPath = setup_cyg_system(target)

    dllver = dllVersion(os.path.join(targetSystemDir(target), target["dll"]))
    packageName = "winpty-" + common_ship.winptyVersion + "-" + target["packageName"].format(dllver=dllver)
    if os.path.exists("ship\\packages\\" + packageName):
        shutil.rmtree("ship\\packages\\" + packageName)

    print("+ Setting PATH to: {}".format(newPath))
    with common_ship.ModifyEnv(PATH=newPath):
        subprocess.check_call(["sh.exe", "configure"])
        subprocess.check_call(["make.exe", "clean"])
        makeBaseCmd = [
            "make.exe",
            "COMMIT_HASH=" + common_ship.commitHash,
            "PREFIX=ship/packages/" + packageName
        ]
        subprocess.check_call(makeBaseCmd + ["all", "tests", "-j%d" % multiprocessing.cpu_count()])
        subprocess.check_call(["build\\trivial_test.exe"])
        subprocess.check_call(makeBaseCmd + ["install"])
        subprocess.check_call(["tar.exe", "cvfz",
            packageName + ".tar.gz",
            packageName], cwd=os.path.join(os.getcwd(), "ship", "packages"))


def main():
    targets = list(BUILD_TARGETS)
    if len(sys.argv) != 1:
        targets = [t for t in targets if t["systemName"] in sys.argv[1:]]

    for t in targets:
        newPath = setup_cyg_system(t)
        with common_ship.ModifyEnv(PATH=newPath):
            subprocess.check_output(["tar.exe", "--help"])
            subprocess.check_output(["make.exe", "--help"])

    for t in targets:
        buildTarget(t)


if __name__ == "__main__":
    main()
