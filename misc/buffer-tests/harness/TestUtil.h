#pragma once

#include <windows.h>

#include <iostream>
#include <string>
#include <tuple>

#include "NtHandleQuery.h"
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
void printTestName(const std::string &name);
void registerTest(const std::string &name, bool(&cond)(), void(&func)());
using RegistrationTable = std::vector<std::tuple<std::string, bool(*)(), void(*)()>>;
RegistrationTable registeredTests();
inline bool always() { return true; }

// NT kernel handle query
void *ntHandlePointer(const std::vector<SYSTEM_HANDLE_ENTRY> &table,
                      RemoteHandle h);
bool hasBuiltinCompareObjectHandles();
bool compareObjectHandles(RemoteHandle h1, RemoteHandle h2);

// NT kernel handle->object snapshot
class ObjectSnap {
public:
    ObjectSnap();
    void *object(RemoteHandle h);
    bool eq(std::initializer_list<RemoteHandle> handles);
    bool eq(RemoteHandle h1, RemoteHandle h2) { return eq({h1, h2}); }
private:
    bool m_hasTable = false;
    std::vector<SYSTEM_HANDLE_ENTRY> m_table;
};

// Misc
std::tuple<RemoteHandle, RemoteHandle> newPipe(
        RemoteWorker &w, BOOL inheritable=FALSE);
std::string windowText(HWND hwnd);

// "domain-specific" routines: perhaps these belong outside the harness?
void checkInitConsoleHandleSet(RemoteWorker &child);
void checkInitConsoleHandleSet(RemoteWorker &child, RemoteWorker &source);
bool isUsableConsoleHandle(RemoteHandle h);
bool isUsableConsoleInputHandle(RemoteHandle h);
bool isUsableConsoleOutputHandle(RemoteHandle h);
bool isUnboundConsoleObject(RemoteHandle h);
void checkModernConsoleHandleInit(RemoteWorker &proc,
                                  bool in, bool out, bool err);
