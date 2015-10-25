#include <TestCommon.h>

REGISTER(Test_CreateProcess_ModeCombos, always);
static void Test_CreateProcess_ModeCombos() {
    // It is often unclear how (or whether) various combinations of
    // CreateProcess parameters work when combined.  Try to test the ambiguous
    // combinations.

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

REGISTER(Test_CreateProcess_STARTUPINFOEX, isAtLeastVista);
static void Test_CreateProcess_STARTUPINFOEX() {
    // STARTUPINFOEX tests.

    Worker p;
    DWORD errCode = 0;
    auto pipe1 = newPipe(p, true);
    auto ph1 = std::get<0>(pipe1);
    auto ph2 = std::get<1>(pipe1);

    auto pipe2 = newPipe(p, true);
    auto ph3 = std::get<0>(pipe2);
    auto ph4 = std::get<1>(pipe2);

    // Add an extra console handle so we can verify that a child's console
    // handles didn't revert to the original default, but were inherited.
    p.openConout(true);

    // Verify that compareObjectHandles is working...
    {
        CHECK(!compareObjectHandles(ph1, ph2));
        auto dupTest = ph1.dup();
        CHECK(compareObjectHandles(ph1, dupTest));
        dupTest.close();
        Worker other;
        CHECK(compareObjectHandles(ph1, ph1.dup(other)));
    }

    auto testSetupOneHandle = [&](SpawnParams sp, size_t cb, HANDLE inherit) {
        sp.sui.cb = cb;
        sp.inheritCount = 1;
        sp.inheritList = { inherit };
        return p.tryChild(sp, &errCode);
    };

    auto testSetupStdHandles = [&](SpawnParams sp) {
        const auto in = sp.sui.hStdInput;
        const auto out = sp.sui.hStdOutput;
        const auto err = sp.sui.hStdError;
        sp.dwCreationFlags |= EXTENDED_STARTUPINFO_PRESENT;
        sp.sui.cb = sizeof(STARTUPINFOEXW);
        // This test case isn't interested in what
        // PROC_THREAD_ATTRIBUTE_HANDLE_LIST does when there are duplicate
        // handles in its list.
        ASSERT(in != out && out != err && in != err);
        sp.inheritCount = 3;
        sp.inheritList = { in, out, err };
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
        CHECK(compareObjectHandles(ph1, ch1));
        CHECK(!compareObjectHandles(ph2, ch2));

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
        CHECK(compareObjectHandles(ph2, ch2));
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
        auto c = testSetupStdHandles({false, 0, {ph1, ph2, ph4}});
        CHECK(!c.valid());
        CHECK_EQ(errCode, (DWORD)ERROR_INVALID_PARAMETER);
    }

    if (!isAtLeastWin8()) {
        // Attempt to restrict inheritance to just one of the three open
        // traditional console handles.
        auto c = testSetupStdHandles({true, 0, {ph1, ph2, p.getStderr()}});
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
            CHECK(!compareObjectHandles(ph1, ch1));
            CHECK(!compareObjectHandles(ph2, ch2));
            CHECK(!compareObjectHandles(ph3, ch3));
            CHECK(!compareObjectHandles(ph4, ch4));
        }
    }

    if (!isAtLeastWin8()) {
        // Make a final valiant effort to find a
        // PROC_THREAD_ATTRIBUTE_HANDLE_LIST and console handle interaction.
        // We'll set all the standard handles to pipes.  Nevertheless, all
        // console handles are inherited.
        auto c = testSetupStdHandles({true, 0, {ph1, ph2, ph4}});
        CHECK(c.valid());
        CHECK(handleValues(c.scanForConsoleHandles()) ==
              handleValues(p.scanForConsoleHandles()));
    }
}

REGISTER(Test_CreateNoWindow_HiddenVsNothing, always);
static void Test_CreateNoWindow_HiddenVsNothing() {

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

// MSDN's CreateProcess page currently has this note in it:
//
//     Important  The caller is responsible for ensuring that the standard
//     handle fields in STARTUPINFO contain valid handle values. These fields
//     are copied unchanged to the child process without validation, even when
//     the dwFlags member specifies STARTF_USESTDHANDLES. Incorrect values can
//     cause the child process to misbehave or crash. Use the Application
//     Verifier runtime verification tool to detect invalid handles.
//
// XXX: The word "even" here sticks out.  Verify that the standard handle
// fields in STARTUPINFO are ignored when STARTF_USESTDHANDLES is not
// specified.
