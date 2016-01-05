// Copyright (c) 2015 Ryan Prichard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef LIBWINPTY_WINPTY_INTERNAL_H
#define LIBWINPTY_WINPTY_INTERNAL_H

#include <windows.h>

#include <string>

#include "../include/winpty.h"
#include "../shared/cxx11_mutex.h"
#include "Util.h"

// The structures in this header are not intended to be accessed directly by
// client programs.

struct winpty_error_s {
    bool errorIsStatic;
    winpty_result_t code;
    LPCWSTR msg;
};

struct winpty_config_s {
    DWORD flags = 0;
    int cols = 80;
    int rows = 25;
    DWORD timeoutMs = 30000;
};

struct winpty_s {
    winpty_cxx11::mutex mutex;
    libwinpty::OwnedHandle agentProcess;
    libwinpty::OwnedHandle controlPipe;
    DWORD agentTimeoutMs = 0;
    libwinpty::OwnedHandle ioEvent;
    std::wstring coninPipeName;
    std::wstring conoutPipeName;
};

struct winpty_spawn_config_s {
    DWORD winptyFlags = 0;
    std::wstring appname;
    std::wstring cmdline;
    std::wstring cwd;
    std::wstring env;
};

#endif // LIBWINPTY_WINPTY_INTERNAL_H
