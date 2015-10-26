#include <TestCommon.h>

//
// Test CreateProcess when called with these parameters:
//  - STARTF_USESTDHANDLES is not specified
//  - bInheritHandles=TRUE
//  - CreationConsoleMode=Inherit
//
// Ordinarily, standard handles are copied as-is.
//
// On Windows 8 and later, if a PROC_THREAD_ATTRIBUTE_HANDLE_LIST list is used,
// then the standard handles are duplicated instead.
//

REGISTER(Test_CreateProcess_InheritAllHandles, always);
static void Test_CreateProcess_InheritAllHandles() {
    auto &hv = handleValues;

    {
        // Simple case: the standard handles are left as-is.
        Worker p;
        auto pipe = newPipe(p, true);
        std::get<0>(pipe).setStdin();
        std::get<1>(pipe).setStdout().setStderr();
        auto c = p.child({ true });
        CHECK(hv(stdHandles(c)) == hv(stdHandles(p)));
    }

    {
        // We can pass arbitrary values through.
        Worker p;
        Handle::invent(0x0ull, p).setStdin();
        Handle::invent(0x10000ull, p).setStdout();
        Handle::invent(INVALID_HANDLE_VALUE, p).setStderr();
        auto c = p.child({ true });
        CHECK(hv(stdHandles(c)) == hv(stdHandles(p)));
    }

    {
        // Passing through a non-inheritable handle produces an invalid child
        // handle.
        Worker p;
        p.openConin(false).setStdin();
        p.openConout(false).setStdout().setStderr();
        auto c = p.child({ true });
        CHECK(hv(stdHandles(c)) == hv(stdHandles(p)));
        if (isTraditionalConio()) {
            CHECK(!c.getStdin().tryFlags());
            CHECK(!c.getStdout().tryFlags());
            CHECK(!c.getStderr().tryFlags());
        } else {
            ObjectSnap snap;
            CHECK(!snap.eq(p.getStdin(), c.getStdin()));
            CHECK(!snap.eq(p.getStdout(), c.getStdout()));
            CHECK(!snap.eq(p.getStderr(), c.getStderr()));
        }
    }
}

REGISTER(Test_CreateProcess_InheritList_StdHandles, isAtLeastVista);
static void Test_CreateProcess_InheritList_StdHandles() {
    // List one of the standard handles in the inherit list, and see what
    // happens to the standard list.

    auto check = [](Worker &p, RemoteHandle rh, RemoteHandle wh) {
        ASSERT(!rh.isTraditionalConsole());
        ASSERT(!wh.isTraditionalConsole());
        {
            // Test bInheritHandles=TRUE, STARTF_USESTDHANDLES, and the
            // PROC_THREAD_ATTRIBUTE_HANDLE_LIST attribute.  Verify that the
            // standard handles are set to handles whose inheritability was
            // suppressed.
            SpawnParams sp { true, EXTENDED_STARTUPINFO_PRESENT, {rh, wh, wh} };
            sp.sui.cb = sizeof(STARTUPINFOEXW);
            sp.inheritCount = 1;
            sp.inheritList = { wh.value() };
            auto c = p.child(sp);
            ObjectSnap snap;
            CHECK(handleValues(stdHandles(c)) ==
                handleValues(std::vector<RemoteHandle> {rh, wh, wh}));
            CHECK(!snap.eq(rh, c.getStdin()));
            CHECK(snap.eq(wh, c.getStdout()));
            CHECK(snap.eq(wh, c.getStderr()));
        }

        if (!isAtLeastWin8()) {
            // Same as above, but avoid STARTF_USESTDHANDLES this time.  The
            // behavior changed with Windows 8, which now appears to duplicate
            // handles in this case.
            rh.setStdin();
            wh.setStdout().setStderr();
            SpawnParams sp { true, EXTENDED_STARTUPINFO_PRESENT };
            sp.sui.cb = sizeof(STARTUPINFOEXW);
            sp.inheritCount = 1;
            sp.inheritList = { wh.value() };
            auto c = p.child(sp);
            ObjectSnap snap;
            CHECK(handleValues(stdHandles(p)) == handleValues(stdHandles(c)));
            CHECK(!snap.eq(p.getStdin(), c.getStdin()));
            CHECK(snap.eq(p.getStdout(), c.getStdout()));
        }
    };

    {
        Worker p;
        auto pipe = newPipe(p, true);
        check(p, std::get<0>(pipe), std::get<1>(pipe));
    }

    if (isModernConio()) {
        Worker p;
        check(p, p.openConin(true), p.openConout(true));
    }
}

REGISTER(Test_CreateProcess_InheritList_ModernDuplication, isAtLeastVista);
static void Test_CreateProcess_InheritList_ModernDuplication() {
    auto &hv = handleValues;

    {
        // Once we've specified an inherit list, non-inheritable standard
        // handles are duplicated.
        Worker p;
        auto pipe = newPipe(p);
        auto rh = std::get<0>(pipe).setStdin();
        auto wh = std::get<1>(pipe).setStdout().setStderr();
        auto c = childWithDummyInheritList(p, {});
        if (isModernConio()) {
            ObjectSnap snap;
            CHECK(snap.eq(rh, c.getStdin()));
            CHECK(snap.eq(wh, c.getStdout()));
            CHECK(snap.eq(wh, c.getStderr()));
            CHECK(c.getStdout().value() != c.getStderr().value());
            for (auto h : stdHandles(c)) {
                CHECK(!h.inheritable());
            }
        } else {
            CHECK(hv(stdHandles(c)) == hv(stdHandles(p)));
            CHECK(!c.getStdin().tryFlags());
            CHECK(!c.getStdout().tryFlags());
            CHECK(!c.getStderr().tryFlags());
        }
    }

    {
        // Invalid handles are translated to 0x0.  (For full details, see the
        // "duplicate" CreateProcess tests.)
        Worker p;
        Handle::invent(0x0ull, p).setStdin();
        Handle::invent(0xdeadbeefull, p).setStdout();
        auto c = childWithDummyInheritList(p, {});
        if (isModernConio()) {
            CHECK(c.getStdin().uvalue() == 0ull);
            CHECK(c.getStdout().uvalue() == 0ull);
        } else {
            CHECK(c.getStdin().uvalue() == 0ull);
            CHECK(c.getStdout().value() ==
                Handle::invent(0xdeadbeefull, c).value());
        }
    }
}

REGISTER(Test_CreateProcess_Duplicate_StdHandles, isModernConio);
static void Test_CreateProcess_Duplicate_StdHandles() {
    // The default Unbound console handles should be inheritable, so with
    // bInheritHandles=TRUE and standard handles listed in the inherit list,
    // the child process should have six console handles, all usable.
    Worker p;

    SpawnParams sp { true, EXTENDED_STARTUPINFO_PRESENT };
    sp.sui.cb = sizeof(STARTUPINFOEXW);
    sp.inheritCount = 3;
    sp.inheritList = {
        p.getStdin().value(),
        p.getStdout().value(),
        p.getStderr().value(),
    };
    auto c = p.child(sp);

    std::vector<uint64_t> expected;
    extendVector(expected, handleInts(stdHandles(p)));
    extendVector(expected, handleInts(stdHandles(c)));
    std::sort(expected.begin(), expected.end());

    auto correct = handleInts(c.scanForConsoleHandles());
    std::sort(correct.begin(), correct.end());

    p.dumpConsoleHandles();
    c.dumpConsoleHandles();

    CHECK(expected == correct);
}
