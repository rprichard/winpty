#!/bin/bash

set -e -x
rm -fr deploy
mkdir deploy
cd deploy

# I expect the QtSDK to be installed and for these two programs
# to be in the PATH:
#   /c/QtSDK/mingw/bin/mingw32-make.exe
#   /c/QtSDK/Desktop/Qt/4.7.4/mingw/bin/qmake.exe

cp ../TestNetServer-build-desktop/debug/TestNetServer.exe .
cp ../Agent-build-desktop/debug/Agent.exe .
cp $(dirname $(which mingw32-make))/{mingwm10.dll,libgcc_s_dw2-1.dll} .
cp $(dirname $(which qmake))/{QtCored4.dll,QtNetworkd4.dll} .
