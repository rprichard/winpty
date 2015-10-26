#include <TestCommon.h>

// With CreateProcess's default inheritance, the GetCurrentProcess()
// psuedo-handle (i.e. INVALID_HANDLE_VALUE) is translated to a real handle
// value for the child process.  It is a handle to the parent process.
// Naturally, this was unintended behavior, and as of Windows 8.1, the handle
// is instead translated to NULL.

REGISTER(Test_CreateProcess_Duplicate_PseudoHandleBug, always);
static void Test_CreateProcess_Duplicate_PseudoHandleBug() {
    Worker p;
    Handle::invent(GetCurrentProcess(), p).setStdout();
    auto c = p.child({ false });
    if (isAtLeastWin8_1()) {
        CHECK(c.getStdout().value() == nullptr);
    } else {
        CHECK(c.getStdout().value() != GetCurrentProcess());
        auto handleToPInP = Handle::dup(p.processHandle(), p);
        CHECK(compareObjectHandles(c.getStdout(), handleToPInP));
    }
}

REGISTER(Test_CreateProcess_Duplicate_PseudoHandleBug_IL, always);
static void Test_CreateProcess_Duplicate_PseudoHandleBug_IL() {
    // As above, but use an inherit list.  With an inherit list, standard
    // handles are duplicated, but only with Windows 8 and up.
    for (int useDummyPipe = 0; useDummyPipe <= 1; ++useDummyPipe) {
        Worker p;
        Handle::invent(INVALID_HANDLE_VALUE, p).setStdout();
        auto c = childWithDummyInheritList(p, {}, useDummyPipe != 0);
        if (isAtLeastWin8_1()) {
            CHECK(c.getStdout().value() == nullptr);
        } else if (isAtLeastWin8()) {
            CHECK(c.getStdout().value() != GetCurrentProcess());
            auto handleToPInP = Handle::dup(p.processHandle(), p);
            CHECK(compareObjectHandles(c.getStdout(), handleToPInP));
        } else {
            CHECK(c.getStdout().value() == INVALID_HANDLE_VALUE);
        }
    }
}
