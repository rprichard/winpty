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

#ifndef OS_MODULE_H
#define OS_MODULE_H

#include <string>

#include "DebugClient.h"

class OsModule {
    std::wstring m_fileName;
    HMODULE m_module;
public:
    OsModule(const wchar_t *fileName) : m_fileName(fileName) {
        m_module = LoadLibraryW(fileName);
        if (m_module == nullptr) {
            trace("Could not load %ls: error %u",
                fileName, static_cast<unsigned>(GetLastError()));
        }
    }
    ~OsModule() {
        if (m_module != nullptr) {
            FreeLibrary(m_module);
        }
    }
    operator bool() const { return m_module != nullptr; }
    HMODULE handle() const { return m_module; }
    FARPROC proc(const char *funcName) {
        FARPROC ret = nullptr;
        if (m_module != nullptr) {
            ret = GetProcAddress(m_module, funcName);
            if (ret == NULL) {
                trace("GetProcAddress: %s is missing from %ls",
                    funcName, m_fileName.c_str());
            }
        }
        return ret;
    }
};

#endif // OS_MODULE_H
