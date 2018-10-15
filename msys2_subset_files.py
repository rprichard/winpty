#!/usr/bin/env python3
import re
import subprocess

groups = set()
packages = {
    # Explicit packages
    'gcc',
    'make',
    'tar',
    'mingw-w64-cross-toolchain',
    # Implicit packages
    'coreutils',
    'gzip',
    'rebase',
}

# mingw-w64-cross-toolchain is a "group" rather than a package, so expand
# it to a list of packages.
for line in subprocess.check_output(['pacman', '-Qg']).decode().splitlines():
    group, member = line.split()
    if group in packages:
        groups.add(group)
        packages.add(member)
packages -= groups

# Transitive closure of dependencies
for p in sorted(packages):
    packages.update(subprocess.check_output(['pactree', '-l', p]).decode().splitlines())

# Get a list of files
paths = set()
for line in subprocess.check_output(['pacman', '-Ql']).decode().splitlines():
    package, path = line.split(' ', 1)
    if package in packages:
        paths.add(path)

# Strip out some unneeded files
unneeded = [
    r'.*/$', # strip out directories
    r'/opt/armv7-w64-mingw32/',
    r'/usr/bin/msys-2\.0\.dbg$',
    r'/(usr|opt)/share/(man|info|locale)/',
    r'/usr/(share|lib|)/terminfo/',
]
unneeded_re = re.compile('(' + '|'.join(unneeded) + ')')

paths = {p for p in paths if not unneeded_re.match(p)}

for x in sorted(paths):
    assert x[0] == '/'
    print(x[1:])
