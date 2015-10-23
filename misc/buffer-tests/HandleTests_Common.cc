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

static void Test_CreateProcess_ModeCombos() {
    // It is often unclear how (or whether) various combinations of
    // CreateProcess parameters work when combined.  Try to test the ambiguous
    // combinations.
    printTestName(__FUNCTION__);

    DWORD errCode = 0;

    {
        // CREATE_NEW_CONSOLE | DETACHED_PROCESS ==> call fails
        Worker p;
        auto c = p.tryChild({ false, CREATE_NEW_CONSOLE | DETACHED_PROCESS }, &errCode);
        CHECK(!c.valid());
        CHECK_EQ(errCode, (DWORD)ERROR_INVALID_PARAMETER);
    }
    {
        // CREATE_NO_WINDOW | CREATE_NEW_CONSOLE ==> CREATE_NEW_CONSOLE dominates
        Worker p;
        auto c = p.tryChild({ false, CREATE_NO_WINDOW | CREATE_NEW_CONSOLE }, &errCode);
        CHECK(c.valid());
        CHECK(c.consoleWindow() != nullptr);
        CHECK(IsWindowVisible(c.consoleWindow()));
    }
    {
        // CREATE_NO_WINDOW | DETACHED_PROCESS ==> DETACHED_PROCESS dominates
        Worker p;
        auto c = p.tryChild({ false, CREATE_NO_WINDOW | DETACHED_PROCESS }, &errCode);
        CHECK(c.valid());
        CHECK_EQ(c.newBuffer().value(), INVALID_HANDLE_VALUE);
    }
}

static void Test_CreateProcess_STARTUPINFOEX() {
    // STARTUPINFOEX tests.
    printTestName(__FUNCTION__);

    Worker p;
    DWORD errCode = 0;
    auto pipe1 = newPipe(p, true);
    auto ph1 = std::get<0>(pipe1);
    auto ph2 = std::get<1>(pipe1);

    auto pipe2 = newPipe(p, true);
    auto ph3 = std::get<0>(pipe2);
    auto ph4 = std::get<1>(pipe2);

    // Add an extra console handle so we can verify that a child's console
    // handles didn't reverted to the original default, but were inherited.
    p.openConout(true);

    // Verify that ntHandlePointer is working...
    CHECK(ntHandlePointer(ph1) != nullptr);
    CHECK(ntHandlePointer(ph2) != nullptr);
    CHECK(ntHandlePointer(ph1) != ntHandlePointer(ph2));
    auto dupTest = ph1.dup();
    CHECK(ntHandlePointer(ph1) == ntHandlePointer(dupTest));
    dupTest.close();

    auto testSetupOneHandle = [&](SpawnParams sp, size_t cb, HANDLE inherit) {
        sp.sui.cb = cb;
        sp.inheritCount = 1;
        sp.inheritList = { inherit };
        return p.tryChild(sp, &errCode);
    };

    auto testSetupStdHandles = [&](SpawnParams sp, Handle in,
                                   Handle out, Handle err) {
        sp.dwCreationFlags |= EXTENDED_STARTUPINFO_PRESENT;
        sp.sui.cb = sizeof(STARTUPINFOEXW);
        sp.sui.dwFlags |= STARTF_USESTDHANDLES;
        sp.sui.hStdInput = in.value();
        sp.sui.hStdOutput = out.value();
        sp.sui.hStdError = err.value();
        // This test case isn't interested in what
        // PROC_THREAD_ATTRIBUTE_HANDLE_LIST does when there are duplicate
        // handles in its list.
        ASSERT(in.value() != out.value() &&
               out.value() != err.value() &&
               in.value() != err.value());
        sp.inheritCount = 3;
        sp.inheritList = {
            sp.sui.hStdInput,
            sp.sui.hStdOutput,
            sp.sui.hStdError,
        };
        return p.tryChild(sp, &errCode);
    };

    {
        // Use PROC_THREAD_ATTRIBUTE_HANDLE_LIST correctly.
        auto c = testSetupOneHandle({true, EXTENDED_STARTUPINFO_PRESENT},
            sizeof(STARTUPINFOEXW), ph1.value());
        CHECK(c.valid());
        auto ch1 = Handle::invent(ph1.value(), c);
        auto ch2 = Handle::invent(ph2.value(), c);
        // i.e. ph1 was inherited, because ch1 identifies the same thing.
        // ph2 was not inherited, because it wasn't listed.
        CHECK(ntHandlePointer(ph1) == ntHandlePointer(ch1));
        CHECK(ntHandlePointer(ph2) != ntHandlePointer(ch2));

        if (!isAtLeastWin8()) {
            // The traditional console handles were all inherited, but they're
            // also the standard handles, so maybe that's an exception.  We'll
            // test more aggressively below.
            CHECK(handleValues(c.scanForConsoleHandles()) ==
                  handleValues(p.scanForConsoleHandles()));
        }
    }
    {
        // The STARTUPINFOEX parameter is ignored if
        // EXTENDED_STARTUPINFO_PRESENT isn't present.
        auto c = testSetupOneHandle({true},
            sizeof(STARTUPINFOEXW), ph1.value());
        CHECK(c.valid());
        auto ch2 = Handle::invent(ph2.value(), c);
        // i.e. ph2 was inherited, because ch2 identifies the same thing.
        CHECK(ntHandlePointer(ph2) == ntHandlePointer(ch2));
    }
    {
        // If EXTENDED_STARTUPINFO_PRESENT is specified, but the cb value
        // is wrong, the API call fails.
        auto c = testSetupOneHandle({true, EXTENDED_STARTUPINFO_PRESENT},
            sizeof(STARTUPINFOW), ph1.value());
        CHECK(!c.valid());
        CHECK_EQ(errCode, (DWORD)ERROR_INVALID_PARAMETER);
    }
    {
        // Attempting to inherit the GetCurrentProcess pseudo-handle also
        // fails.  (The MSDN docs point out that using GetCurrentProcess here
        // will fail.)
        auto c = testSetupOneHandle({true, EXTENDED_STARTUPINFO_PRESENT},
            sizeof(STARTUPINFOEXW), GetCurrentProcess());
        CHECK(!c.valid());
        CHECK_EQ(errCode, (DWORD)ERROR_INVALID_PARAMETER);
    }
    {
        // If bInheritHandles=FALSE and PROC_THREAD_ATTRIBUTE_HANDLE_LIST are
        // combined, the API call fails.
        auto c = testSetupStdHandles({false}, ph1, ph2, ph4);
        CHECK(!c.valid());
        CHECK_EQ(errCode, (DWORD)ERROR_INVALID_PARAMETER);
    }

    if (!isAtLeastWin8()) {
        // Attempt to restrict inheritance to just one of the three open
        // traditional console handles.
        auto c = testSetupStdHandles({true}, ph1, ph2, p.getStderr());
        if (isWin7()) {
            // On Windows 7, the CreateProcess call fails with a strange
            // error.
            CHECK(!c.valid());
            CHECK_EQ(errCode, (DWORD)ERROR_NO_SYSTEM_RESOURCES);
        } else {
            // On Vista, the CreateProcess call succeeds, but handle
            // inheritance is broken.  All of the console handles are
            // inherited, not just the error screen buffer that was listed.
            // None of the pipe handles were inherited, even though two were
            // listed.
            c.dumpConsoleHandles();
            CHECK(handleValues(c.scanForConsoleHandles()) ==
                  handleValues(p.scanForConsoleHandles()));
            auto ch1 = Handle::invent(ph1.value(), c);
            auto ch2 = Handle::invent(ph2.value(), c);
            auto ch3 = Handle::invent(ph3.value(), c);
            auto ch4 = Handle::invent(ph4.value(), c);
            CHECK(ntHandlePointer(ph1) != ntHandlePointer(ch1));
            CHECK(ntHandlePointer(ph2) != ntHandlePointer(ch2));
            CHECK(ntHandlePointer(ph3) != ntHandlePointer(ch3));
            CHECK(ntHandlePointer(ph4) != ntHandlePointer(ch4));
        }
    }

    if (!isAtLeastWin8()) {
        // Make a final valiant effort to test
        // PROC_THREAD_ATTRIBUTE_HANDLE_LIST and console handle interaction.
        // We'll set all the standard handles to pipes.  Nevertheless, all
        // console handles are inherited.
        auto c = testSetupStdHandles({true}, ph1, ph2, ph4);
        CHECK(c.valid());
        CHECK(handleValues(c.scanForConsoleHandles()) ==
              handleValues(p.scanForConsoleHandles()));
    }
}

static void Test_CreateNoWindow_HiddenVsNothing() {
    printTestName(__FUNCTION__);

    Worker p;
    auto c = p.child({ false, CREATE_NO_WINDOW });

    if (isAtLeastWin7()) {
        // As of Windows 7, GetConsoleWindow returns NULL.
        CHECK(c.consoleWindow() == nullptr);
    } else {
        // On earlier operating systems, GetConsoleWindow returns a handle
        // to an invisible window.
        CHECK(c.consoleWindow() != nullptr);
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
        SpawnParams sp(true);
        sp.sui.dwFlags |= STARTF_USESTDHANDLES;
        sp.sui.hStdInput = std::get<0>(pipe).value();
        sp.sui.hStdOutput = std::get<1>(pipe).value();
        sp.sui.hStdError = std::get<1>(pipe).dup(true).value();
        auto p3c = p3.child(sp);
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
    Test_CreateProcess_ModeCombos();
    if (isAtLeastVista()) {
        Test_CreateProcess_STARTUPINFOEX();
    }
    Test_CreateNoWindow_HiddenVsNothing();
    Test_Input_Vs_Output();
    Test_Detach_Does_Not_Change_Standard_Handles();
    Test_Activate_Does_Not_Change_Standard_Handles();
    Test_Active_ScreenBuffer_Order();
    Test_GetStdHandle_SetStdHandle();
}
