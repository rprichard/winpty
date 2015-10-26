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
