import sys

# Install Python from https://www.python.org.
(sys.version_info[0:2] >= (3, 5))   or sys.exit('error: script requires Python 3.5 or above')

import os
import re
import shutil
import stat
import subprocess

from glob import glob
from datetime import datetime


def glob_paths(patterns):
    ret = []
    for p in patterns:
        batch = glob(p.replace('/', '\\'))
        if len(batch) == 0:
            sys.exit('error: pattern matched no files: {}'.format(p))
        ret.extend(batch)
    return ret


def rmpath(path):
    if os.path.islink(path):
        os.remove(path)
    elif os.path.isfile(path):
        # MSYS2 makes files read-only, and we need this chmod command to delete
        # them.
        os.chmod(path, stat.S_IWRITE)
        os.remove(path)
    elif os.path.isdir(path):
        for child in os.listdir(path):
            # listdir excludes '.' and '..'
            rmpath(os.path.join(path, child))
        os.rmdir(path)



def mkdirs(path):
    if not os.path.isdir(path):
        os.makedirs(path)


def getGppVer(*cmd, **kwargs):
    txt = subprocess.check_output(*cmd, **kwargs).decode()
    txt = txt.splitlines()[0]
    # Version strings we're trying to match:
    #  - cygwin/msys2 gcc: g++ (GCC) 7.3.0
    #  - cygwin mingw gcc: x86_64-w64-mingw32-g++ (GCC) 6.4.0
    #  - msys2 mingw gcc: x86_64-w64-mingw32-g++ (Rev3, Built by MSYS2 project) 8.2.0
    m = re.search(r'(?:GCC|Built by MSYS2 project)\) (\d+\.\d+\.\d+)$', txt)
    if not m:
        sys.exit('error: g++ version did not match pattern: {}'.format(repr(txt)))
    return m.group(1)


buildTimeStamp = datetime.now().strftime('%Y%m%d')
projectDir = os.path.dirname(os.path.abspath(__file__))
