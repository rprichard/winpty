import json
import os
import subprocess
import sys


def versionDict(dllpath):
    if not os.path.isfile(dllpath):
        sys.exit('error: {} is not an existing file'.format(dllpath))
    txt = subprocess.check_output([
        'powershell',
        '[System.Diagnostics.FileVersionInfo]::GetVersionInfo("{}") | ConvertTo-Json'.format(dllpath.replace('\\', '/')),
    ])
    return json.loads(txt)


def fileVersion(dllpath):
    ret = versionDict(dllpath)['FileVersion']
    assert ret not in (None, '')
    return ret
