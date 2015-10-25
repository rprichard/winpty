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
        auto handles1 = handleValues(stdHandles(p));
        p.detach();
        auto handles2 = handleValues(stdHandles(p));
        CHECK(handles1 == handles2);
    };
    // Simplest form of the test.
    {
        Worker p1;
        check(p1);
    }
    // Also do a test with duplicated handles, just in case detaching resets
    // the handles to their defaults.
    {
        Worker p2;
        p2.getStdin().dup(TRUE).setStdin();
        p2.getStdout().dup(TRUE).setStdout();
        p2.getStderr().dup(TRUE).setStderr();
        check(p2);
    }
    // Do another test with STARTF_USESTDHANDLES, just in case detaching resets
    // to the hStd{Input,Output,Error} values.
    {
        Worker p3;
        auto pipe = newPipe(p3, true);
        auto rh = std::get<0>(pipe);
        auto wh = std::get<1>(pipe);
        auto p3c = p3.child({true, 0, {rh, wh, wh.dup(true)}});
        check(p3c);
    }
}

static void Test_Activate_Does_Not_Change_Standard_Handles() {
    // SetConsoleActiveScreenBuffer does not change the standard handles.
    // MSDN documents this fact on "Console Handles"[1]
    //
    //     "Note that changing the active screen buffer does not affect the
    //     handle returned by GetStdHandle. Similarly, using SetStdHandle to
    //     change the STDOUT handle does not affect the active screen buffer."
    //
    // [1] https://msdn.microsoft.com/en-us/library/windows/desktop/ms682075.aspx
    printTestName(__FUNCTION__);
    Worker p;
    auto handles1 = handleValues(stdHandles(p));
    p.newBuffer(TRUE).activate();
    auto handles2 = handleValues(stdHandles(p));
    CHECK(handles1 == handles2);
}

static void Test_Active_ScreenBuffer_Order() {
    // SetActiveConsoleScreenBuffer does not increase a refcount on the
    // screen buffer.  Instead, when the active screen buffer's refcount hits
    // zero, Windows activates the most-recently-activated buffer.

    auto firstChar = [](Worker &p) {
        auto h = p.openConout();
        auto ret = h.firstChar();
        h.close();
        return ret;
    };

    printTestName(__FUNCTION__);
    {
        // Simplest test
        Worker p;
        p.getStdout().setFirstChar('a');
        auto h = p.newBuffer(false, 'b').activate();
        h.close();
        CHECK_EQ(firstChar(p), 'a');
    }
    {
        // a -> b -> c -> b -> a
        Worker p;
        p.getStdout().setFirstChar('a');
        auto b = p.newBuffer(false, 'b').activate();
        auto c = p.newBuffer(false, 'c').activate();
        c.close();
        CHECK_EQ(firstChar(p), 'b');
        b.close();
        CHECK_EQ(firstChar(p), 'a');
    }
    {
        // a -> b -> c -> b -> c -> a
        Worker p;
        p.getStdout().setFirstChar('a');
        auto b = p.newBuffer(false, 'b').activate();
        auto c = p.newBuffer(false, 'c').activate();
        b.activate();
        b.close();
        CHECK_EQ(firstChar(p), 'c');
        c.close();
        CHECK_EQ(firstChar(p), 'a');
    }
}

static void Test_GetStdHandle_SetStdHandle() {
    // A commenter on the Old New Thing blog suggested that
    // GetStdHandle/SetStdHandle could have internally used CloseHandle and/or
    // DuplicateHandle, which would have changed the resource management
    // obligations of the callers to those APIs.  In fact, the APIs are just
    // simple wrappers around global variables.  Try to write tests for this
    // fact.
    //
    // http://blogs.msdn.com/b/oldnewthing/archive/2013/03/07/10399690.aspx#10400489
    printTestName(__FUNCTION__);
    auto &hv = handleValues;
    {
        // Set values and read them back.  We get the same handles.
        Worker p;
        auto pipe = newPipe(p);
        auto rh = std::get<0>(pipe);
        auto wh1 = std::get<1>(pipe);
        auto wh2 = std::get<1>(pipe).dup();
        setStdHandles({ rh, wh1, wh2 });
        CHECK(hv(stdHandles(p)) == hv({ rh, wh1, wh2}));

        // Call again, and we still get the same handles.
        CHECK(hv(stdHandles(p)) == hv({ rh, wh1, wh2}));
    }
    {
        Worker p;
        p.getStdout().setFirstChar('a');
        p.newBuffer(false, 'b').activate().setStdout().dup().setStderr();
        std::get<1>(newPipe(p)).setStdout().dup().setStderr();

        // SetStdHandle doesn't close its previous handle when it's given a new
        // handle.  Therefore, the two handles given to SetStdHandle for STDOUT
        // and STDERR are still open, and the new screen buffer is still
        // active.
        CHECK_EQ(p.openConout().firstChar(), 'b');
    }
}

void runCommonTests() {
    Test_IntrinsicInheritFlags();
    Test_Input_Vs_Output();
    Test_Detach_Does_Not_Change_Standard_Handles();
    Test_Activate_Does_Not_Change_Standard_Handles();
    Test_Active_ScreenBuffer_Order();
    Test_GetStdHandle_SetStdHandle();
}
