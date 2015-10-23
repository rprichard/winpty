#include <TestCommon.h>

// Verify that the child's open console handle set is as expected from having
// just attached to or spawned from a source worker.
//  * The set of child handles should exactly match the set of inheritable
//    source handles.
//  * Every open child handle should be inheritable.
static void checkAttachHandleSet(Worker &child, Worker &source) {
    auto cvec = child.scanForConsoleHandles();
    auto cvecInherit = inheritableHandles(cvec);
    auto svecInherit = inheritableHandles(source.scanForConsoleHandles());
    auto hv = &handleValues;
    if (hv(cvecInherit) == hv(svecInherit) && allInheritable(cvec)) {
        return;
    }
    source.dumpConsoleHandles();
    child.dumpConsoleHandles();
    CHECK(false && "checkAttachHandleSet failed");
}

static void Test_NewConsole_Resets_Everything() {
    // Test that creating a new console properly resets everything.
    //
    // Tests:
    //  * CreateProcess - CREATE_NEW_CONSOLE
    //  * AllocConsole
    //
    printTestName(__FUNCTION__);
    Worker p;

    // Open some handles to demonstrate the "clean slate" outcome.
    auto orig = { p.getStdin(), p.getStdout(), p.getStderr() };
    p.getStdin().dup(TRUE).setStdin();
    p.newBuffer(TRUE).setStderr().dup(TRUE).setStdout().activate();
    for (auto &h : orig) {
        Handle::invent(h.value(), p).close();
    }

    auto checkClean = [](Worker &proc) {
        proc.dumpConsoleHandles();
        CHECK_EQ(proc.getStdin().uvalue(), 0x3u);
        CHECK_EQ(proc.getStdout().uvalue(), 0x7u);
        CHECK_EQ(proc.getStderr().uvalue(), 0xbu);
        auto handles = proc.scanForConsoleHandles();
        CHECK(handleValues(handles) == (std::vector<HANDLE> {
            proc.getStdin().value(),
            proc.getStdout().value(),
            proc.getStderr().value(),
        }));
        for (auto &h : handles) {
            CHECK(h.inheritable());
        }
    };

    // A child with a new console is reset to a blank slate.
    auto c = p.child({ true, CREATE_NEW_CONSOLE });
    checkClean(c);

    // Similarly, detaching and allocating a new console resets everything.
    p.detach();
    p.alloc();
    checkClean(p);
}

static void Test_DetachedProcess() {
    // A child with DETACHED_PROCESS has no console, and its standard handles
    // are set to 0 by default.
    printTestName(__FUNCTION__);
    Worker p;

    p.getStdin().dup(TRUE).setStdin();
    p.getStdout().dup(TRUE).setStdout();
    p.getStderr().dup(TRUE).setStderr();

    auto c = p.child({ true, DETACHED_PROCESS });

    CHECK(c.getStdin().uvalue() == 0);
    CHECK(c.getStdout().uvalue() == 0);
    CHECK(c.getStderr().uvalue() == 0);
    CHECK(c.scanForConsoleHandles().empty());
    CHECK(c.consoleWindow() == NULL);

    // XXX: With bInheritHandles=TRUE and DETACHED_PROCESS, are the standard handles always reset?
    //  - There are multiple cases to consider.  i.e. Omitting STARTF_USESTDHANDLES vs specifying it.
    // XXX: What do GetConsoleCP and GetConsoleOutputCP do when no console is attached?

    // Verify that we have a blank slate even with an implicit console
    // creation.
    auto c2 = c.child({ true });
    auto c2h = c2.scanForConsoleHandles();
    CHECK(handleValues(c2h) == (std::vector<HANDLE> {
        c2.getStdin().value(),
        c2.getStdout().value(),
        c2.getStderr().value(),
    }));
}

static void Test_Creation_bInheritHandles_Flag() {
    // The bInheritHandles flags to CreateProcess has no effect on console
    // handles.
    // XXX: I think it *does* on Windows 8 and up.
    printTestName(__FUNCTION__);
    Worker p;
    for (auto &h : (Handle[]){
        p.getStdin(),
        p.getStdout(),
        p.getStderr(),
        p.newBuffer(FALSE),
        p.newBuffer(TRUE),
    }) {
        h.dup(FALSE);
        h.dup(TRUE);
    }
    auto cY = p.child({ true });
    auto cN = p.child({ false });
    CHECK(handleValues(cY.scanForConsoleHandles()) ==
          handleValues(cN.scanForConsoleHandles()));
}

static void Test_HandleAllocationOrder() {
    // When a new handle is created, it always assumes the lowest unused value.
    printTestName(__FUNCTION__);
    Worker p;

    auto h3 = p.getStdin();
    auto h7 = p.getStdout();
    auto hb = p.getStderr();
    auto hf = h7.dup(TRUE);
    auto h13 = h3.dup(TRUE);
    auto h17 = hb.dup(TRUE);

    CHECK(h3.uvalue() == 0x3);
    CHECK(h7.uvalue() == 0x7);
    CHECK(hb.uvalue() == 0xb);
    CHECK(hf.uvalue() == 0xf);
    CHECK(h13.uvalue() == 0x13);
    CHECK(h17.uvalue() == 0x17);

    hf.close();
    h13.close();
    h7.close();

    h7 = h3.dup(TRUE);
    hf = h3.dup(TRUE);
    h13 = h3.dup(TRUE);
    auto h1b = h3.dup(TRUE);

    CHECK(h7.uvalue() == 0x7);
    CHECK(hf.uvalue() == 0xf);
    CHECK(h13.uvalue() == 0x13);
    CHECK(h1b.uvalue() == 0x1b);
}

static void Test_InheritNothing() {
    // It's possible for the standard handles to be non-inheritable.
    //
    // Avoid calling DuplicateHandle(h, FALSE), because it produces inheritable
    // console handles on Windows 7.
    printTestName(__FUNCTION__);
    Worker p;
    auto conin = p.openConin();
    auto conout = p.openConout();
    p.getStdin().close();
    p.getStdout().close();
    p.getStderr().close();
    conin.setStdin();
    conout.setStdout().dup().setStderr();
    p.dumpConsoleHandles();

    auto c = p.child({ true });
    // The child has no open console handles.
    CHECK(c.scanForConsoleHandles().empty());
    c.dumpConsoleHandles();
    // The standard handle values are inherited, even though they're invalid.
    CHECK(c.getStdin().value() == p.getStdin().value());
    CHECK(c.getStdout().value() == p.getStdout().value());
    CHECK(c.getStderr().value() == p.getStderr().value());
    // Verify a console is attached.
    CHECK(c.openConin().value() != INVALID_HANDLE_VALUE);
    CHECK(c.openConout().value() != INVALID_HANDLE_VALUE);
    CHECK(c.newBuffer().value() != INVALID_HANDLE_VALUE);
}

// XXX: Does specifying a handle in STD_{...}_HANDLE or hStd{Input,...}
// influence whether it is inherited, in any situation?

static void Test_AttachConsole_And_CreateProcess_Inheritance() {
    printTestName(__FUNCTION__);
    Worker p;
    Worker unrelated(SpawnParams { false, DETACHED_PROCESS });

    auto conin = p.getStdin().dup(TRUE);
    auto conout1 = p.getStdout().dup(TRUE);
    auto conout2 = p.getStderr().dup(TRUE);
    p.openConout(FALSE);    // an extra handle for checkAttachHandleSet testing
    p.openConout(TRUE);     // an extra handle for checkAttachHandleSet testing
    p.getStdin().close();
    p.getStdout().close();
    p.getStderr().close();
    conin.setStdin();
    conout1.setStdout();
    conout2.setStderr();

    auto c = p.child({ true });

    auto c2 = c.child({ true });
    c2.detach();
    c2.attach(c);

    unrelated.attach(p);

    // The first child will have the same standard handles as the parent.
    CHECK(c.getStdin().value() == p.getStdin().value());
    CHECK(c.getStdout().value() == p.getStdout().value());
    CHECK(c.getStderr().value() == p.getStderr().value());

    // AttachConsole always sets the handles to (0x3, 0x7, 0xb) regardless of
    // handle validity.  In this case, c2 initially had non-default handles,
    // and it attached to a process that has and also initially had
    // non-default handles.  Nevertheless, the new standard handles are the
    // defaults.
    for (auto proc : {&c2, &unrelated}) {
        CHECK(proc->getStdin().uvalue() == 0x3);
        CHECK(proc->getStdout().uvalue() == 0x7);
        CHECK(proc->getStderr().uvalue() == 0xb);
    }

    // The set of inheritable console handles in the two children exactly match
    // that of the parent.
    checkAttachHandleSet(c, p);
    checkAttachHandleSet(c2, p);
    checkAttachHandleSet(unrelated, p);
}

// XXX: This isn't quite ideal.  FreeConsole's behavior is different:
//  - traditional: wipe out ConsoleHandleSet, leave std handles alone
//  - modern: close handles opened on attachment if any, leave std handles alone

static void Test_Detach_Implicitly_Closes_Handles() {
    // After detaching, calling GetHandleInformation fails on previous console
    // handles.  Prior to Windows 8, this property applied to all console
    // handles; in Windows 8, it only applies to some handles, apparently.

    printTestName(__FUNCTION__);
    Worker p;
    Handle orig1[] = {
        p.getStdin(),
        p.getStdout(),
        p.getStderr(),
    };
    Handle orig2[] = {
        p.getStdin().dup(TRUE),
        p.getStdout().dup(TRUE),
        p.getStderr().dup(TRUE),
        p.openConin(TRUE),
        p.openConout(TRUE),
    };

    p.detach();

    // After detaching the console, these handles are closed.
    for (auto h : orig1) {
        CHECK(!h.tryFlags());
    }
    if (!isAtLeastWin8()) {
        // Previously, these handles were also closed.
        for (auto h : orig2) {
            CHECK(!h.tryFlags());
        }
    } else {
        // As of Windows 8, these handles aren't closed.
        for (auto h : orig2) {
            CHECK(h.inheritable());
        }
    }
}

void runTraditionalTests() {
    Test_NewConsole_Resets_Everything();
    Test_DetachedProcess();
    Test_Creation_bInheritHandles_Flag();
    Test_HandleAllocationOrder();
    Test_InheritNothing();
    Test_AttachConsole_And_CreateProcess_Inheritance();
    Test_Detach_Implicitly_Closes_Handles();
}
