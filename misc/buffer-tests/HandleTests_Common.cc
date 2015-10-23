#include <TestCommon.h>

static void Test_IntrinsicInheritFlags() {
    // Console handles have an inherit flag, just as kernel handles do.
    //
    // In Windows 7, there is a bug where DuplicateHandle(h, FALSE) makes the
    // new handle inheritable if the old handle was inheritable.
    printTestName(__FUNCTION__);

    Worker p;
    auto n =  p.newBuffer(FALSE);
    auto y =  p.newBuffer(TRUE);
    auto nn = n.dup(FALSE);
    auto yn = y.dup(FALSE);
    auto ny = n.dup(TRUE);
    auto yy = y.dup(TRUE);
    p.dumpConsoleHandles();

    CHECK(n.inheritable()  == false);
    CHECK(nn.inheritable() == false);
    CHECK(yn.inheritable() == isWin7());
    CHECK(y.inheritable()  == true);
    CHECK(ny.inheritable() == true);
    CHECK(yy.inheritable() == true);

    for (auto &h : (Handle[]){ n, y, nn, ny, yn, yy }) {
        const bool v = h.inheritable();
        if (isWin7()) {
            // In Windows 7, the console handle inherit flags could not be
            // changed.
            CHECK(h.trySetInheritable(v) == false);
            CHECK(h.trySetInheritable(!v) == false);
            CHECK(h.inheritable() == v);
        } else {
            // With older and newer operating systems, the inheritability can
            // be changed.  (In newer operating systems, i.e. Windows 8 and up,
            // the console handles are just normal kernel handles.)
            CHECK(h.trySetInheritable(!v) == true);
            CHECK(h.inheritable() == !v);
        }
    }
    p.dumpConsoleHandles();

    // For sanity's sake, check that DuplicateHandle(h, FALSE) does the right
    // thing with an inheritable pipe handle, even on Windows 7.
    auto pipeY = std::get<0>(newPipe(p, TRUE));
    auto pipeN = pipeY.dup(FALSE);
    CHECK(pipeY.inheritable() == true);
    CHECK(pipeN.inheritable() == false);
}

// XXX: Test that CREATE_NEW_CONSOLE + DETACHED_PROCESS ==> failure
// XXX: Test that CREATE_NEW_CONSOLE + CREATE_NO_WINDOW ==> CREATE_NEW_CONSOLE
// XXX: Test that DETACHED_PROCESS + CREATE_NO_WINDOW ==> DETACHED_PROCESS

static void Test_CreateNoWindow() {
    printTestName(__FUNCTION__);

    Worker p;
    // Open some handles to demonstrate the "clean slate" outcome.
    p.getStdin().dup(TRUE).setStdin();
    p.newBuffer(TRUE).setStderr().dup(TRUE).setStdout().activate();
    auto c = p.child({ true, CREATE_NO_WINDOW });
    c.dumpConsoleHandles();
    // Verify a blank slate.
    auto handles = c.scanForConsoleHandles();
    CHECK(handleValues(handles) == (std::vector<HANDLE> {
        c.getStdin().value(),
        c.getStdout().value(),
        c.getStderr().value(),
    }));
    if (isAtLeastWin7()) {
        // As of Windows 7, there is no console window.
        CHECK(c.consoleWindow() == NULL);
    } else {
        // On earlier operating systems, there is a console window, but it's
        // hidden.
        CHECK(!IsWindowVisible(c.consoleWindow()));
    }
}

static void Test_Input_Vs_Output() {
    // Ensure that APIs meant for the other kind of handle fail.
    printTestName(__FUNCTION__);
    Worker p;
    CHECK(!p.getStdin().tryScreenBufferInfo());
    CHECK(!p.getStdout().tryNumberOfConsoleInputEvents());
}

static void Test_Detach_Does_Not_Change_Standard_Handles() {
    // Detaching the current console does not affect the standard handles.
    printTestName(__FUNCTION__);
    auto check = [](Worker &p) {
        Handle h[] = { p.getStdin(), p.getStdout(), p.getStderr() };
        p.detach();
        CHECK(p.getStdin().value() == h[0].value());
        CHECK(p.getStdout().value() == h[1].value());
        CHECK(p.getStderr().value() == h[2].value());
    };
    // Simplest form of the test.
    Worker p1;
    check(p1);
    // Also do a test with duplicated handles, just in case detaching resets
    // the handles to their defaults.
    Worker p2;
    p2.getStdin().dup(TRUE).setStdin();
    p2.getStdout().dup(TRUE).setStdout();
    p2.getStderr().dup(TRUE).setStderr();
    check(p2);
}

static void Test_Activate_Does_Not_Change_Standard_Handles() {
    // Setting a console as active does not change the standard handles.
    printTestName(__FUNCTION__);
    Worker p;
    auto out = p.getStdout();
    auto err = p.getStderr();
    p.newBuffer(TRUE).activate();
    CHECK(p.getStdout().value() == out.value());
    CHECK(p.getStderr().value() == err.value());
}

void runCommonTests() {
    Test_IntrinsicInheritFlags();
    Test_CreateNoWindow();
    Test_Input_Vs_Output();
    Test_Detach_Does_Not_Change_Standard_Handles();
    Test_Activate_Does_Not_Change_Standard_Handles();
}
