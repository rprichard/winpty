#pragma once

#include <windows.h>

#include <iostream>
#include <string>
#include <tuple>

#include "RemoteHandle.h"

class RemoteWorker;

#define CHECK(cond) \
    do {                                                                      \
        if (!(cond)) {                                                        \
            trace("%s:%d: ERROR: check failed: " #cond, __FILE__, __LINE__);  \
            std::cout << __FILE__ << ":" << __LINE__                          \
                      << (": ERROR: check failed: " #cond)                    \
                      << std::endl;                                           \
        }                                                                     \
    } while(0)

#define CHECK_EQ(actual, expected) \
    do {                                                                      \
        auto a = (actual);                                                    \
        auto e = (expected);                                                  \
        if (a != e) {                                                         \
            trace("%s:%d: ERROR: check failed "                               \
                  "(" #actual " != " #expected ")", __FILE__, __LINE__);      \
            std::cout << __FILE__ << ":" << __LINE__                          \
                      << ": ERROR: check failed "                             \
                      << ("(" #actual " != " #expected "): ")                 \
                      << a << " != " << e                                     \
                      << std::endl;                                           \
        }                                                                     \
    } while(0)

#define REGISTER(name, cond) \
    static void name(); \
    int g_register_ ## cond ## _ ## name = (registerTest(#name, cond, name), 0)

// Test registration
void registerTest(const std::string &name, bool(&cond)(), void(&func)());
using RegistrationTable = std::vector<std::tuple<std::string, bool(*)(), void(*)()>>;
RegistrationTable registeredTests();
inline bool always() { return true; }
void printTestName(const std::string &name);

// NT kernel handle query
void *ntHandlePointer(RemoteHandle h);
bool compareObjectHandles(RemoteHandle h1, RemoteHandle h2);

// Misc
std::tuple<RemoteHandle, RemoteHandle> newPipe(
        RemoteWorker &w, BOOL inheritable=FALSE);
std::string windowText(HWND hwnd);
