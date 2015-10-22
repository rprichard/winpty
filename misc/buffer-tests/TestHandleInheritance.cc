// Console handle tests
//
// These tests should pass on all client and server OSs from XP/2003 to
// Windows 10, inclusive.
//

#include <cassert>
#include <iostream>
#include <tuple>
#include <vector>

#include <TestCommon.h>
#include <OsVersion.h>

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::cout << __FILE__ << ":" << __LINE__ \
                      << ": ERROR: check failed: " \
                      << #cond << std::endl; \
        } \
    } while(0)

static bool inheritable(Handle &h) {
    return h.flags() & HANDLE_FLAG_INHERIT;
}

static bool trySetInheritable(Handle &h, bool inheritable) {
    return h.trySetFlags(HANDLE_FLAG_INHERIT,
                         inheritable ? HANDLE_FLAG_INHERIT : 0);
}

static std::vector<Handle> inheritableHandles(const std::vector<Handle> &vec) {
    std::vector<Handle> ret;
    for (auto h : vec) {
        if (inheritable(h)) {
            ret.push_back(h);
        }
    }
    return ret;
}

static std::vector<uint64_t> handleInts(const std::vector<Handle> &vec) {
    std::vector<uint64_t> ret;
    for (auto h : vec) {
        ret.push_back(reinterpret_cast<uint64_t>(h.value()));
    }
    return ret;
}

static std::vector<HANDLE> handleValues(const std::vector<Handle> &vec) {
    std::vector<HANDLE> ret;
    for (auto h : vec) {
        ret.push_back(h.value());
    }
    return ret;
}

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
    if (hv(cvecInherit) == hv(svecInherit) && hv(cvecInherit) == hv(cvec)) {
        return;
    }
    source.dumpConsoleHandles();
    child.dumpConsoleHandles();
    CHECK(false && "checkAttachHandleSet failed");
}

static std::tuple<Handle, Handle> newPipe(Worker &w, BOOL inheritable=FALSE) {
    HANDLE readPipe, writePipe;
    auto ret = CreatePipe(&readPipe, &writePipe, NULL, 0);
    assert(ret && "CreatePipe failed");
    auto p1 = Handle::dup(readPipe, w, inheritable);
    auto p2 = Handle::dup(writePipe, w, inheritable);
    return std::make_tuple(p1, p2);
}

static void printTestName(const char *testName) {
    trace("----------------------------------------------------------");
    trace("%s", testName);
    printf("%s\n", testName);
    fflush(stdout);
}

static void Test_IntrinsicInheritFlags() {
    // Console handles have an inherit flag, just as kernel handles do.
    //
    // In Windows 7, there is a bug where DuplicateHandle(h, FALSE) makes the
    // new handle inheritable if the old handle was inheritable.
    printTestName("testIntrinsicInheritFlags");

    Worker p;
    auto n =  p.newBuffer(FALSE);
    auto y =  p.newBuffer(TRUE);
    auto nn = n.dup(FALSE);
    auto yn = y.dup(FALSE);
    auto ny = n.dup(TRUE);
    auto yy = y.dup(TRUE);
    p.dumpConsoleHandles();

    CHECK(inheritable(n)  == false);
    CHECK(inheritable(nn) == false);
    CHECK(inheritable(yn) == isWin7());
    CHECK(inheritable(y)  == true);
    CHECK(inheritable(ny) == true);
    CHECK(inheritable(yy) == true);

    for (auto &h : (Handle[]){ n, y, nn, ny, yn, yy }) {
        const bool v = inheritable(h);
        if (isWin7()) {
            // In Windows 7, the console handle inherit flags could not be
            // changed.
            CHECK(trySetInheritable(h, v) == false);
            CHECK(trySetInheritable(h, !v) == false);
            CHECK(inheritable(h) == v);
        } else {
            // With older and newer operating systems, the inheritability can
            // be changed.  (In newer operating systems, i.e. Windows 8 and up,
            // the console handles are just normal kernel handles.)
            CHECK(trySetInheritable(h, !v) == true);
            CHECK(inheritable(h) == !v);
        }
    }
    p.dumpConsoleHandles();

    // For sanity's sake, check that DuplicateHandle(h, FALSE) does the right
    // thing with an inheritable pipe handle, even on Windows 7.
    auto pipeY = std::get<0>(newPipe(p, TRUE));
    auto pipeN = pipeY.dup(FALSE);
    CHECK(inheritable(pipeY) == true);
    CHECK(inheritable(pipeN) == false);
}

//
// Test "traditional" console handle inheritance, as it worked prior to
// Windows 8.  Details:
//
//  * During console initialization (either process startup or AttachConsole),
//    the process assumes the exact set of inheritable console handles as are
//    open in the source process (either parent or attachee), with the same
//    values.
//
//  * AttachConsole resets the STD_{INPUT,OUTPUT,ERROR} handles to exactly
//    0x3, 0x7, and 0xb, even if those handles are invalid or if the previous
//    standard handles were non-console handles.
//
//  * FreeConsole has no effect on the standard handles.  After FreeConsole
//    returns, there is no console attached, so console handles are useless.
//    They are closed, effectively or actually.
//
//  * XXX: How does process startup set the standard handles?
//  * XXX: Potential interactions could come from:
//      - dwCreationFlags:
//          CREATE_NEW_CONSOLE
//          CREATE_NEW_PROCESS_GROUP
//          CREATE_NO_WINDOW
//          DETACHED_PROCESS
//          EXTENDED_STARTUPINFO_PRESENT
//      - STARTUPINFO
//          dwFlags: STARTF_USESTDHANDLES
//          hStdInput
//          hStdOutput
//          hStdError
//      - GetStdHandle(STD_{INPUT,OUTPUT,ERROR}_HANDLE)
//

static void Test_CreateNoWindow() {
    // The documentation for CREATE_NO_WINDOW is confusing:
    //
    //   The process is a console application that is being run without a
    //   console window. Therefore, the console handle for the application is
    //   not set.
    //
    //   This flag is ignored if the application is not a console application,
    //   or if it is used with either CREATE_NEW_CONSOLE or DETACHED_PROCESS.
    //
    // Here's what's evident from examining the OS behavior:
    //
    //  * Specifying both CREATE_NEW_CONSOLE and DETACHED_PROCESS causes the
    //    CreateProcess call to fail.
    //
    //  * If CREATE_NO_WINDOW is specified together with CREATE_NEW_CONSOLE or
    //    DETACHED_PROCESS, it is quietly ignored, just as documented.
    //
    //  * Otherwise, CreateProcess behaves the same with CREATE_NO_WINDOW as it
    //    does with CREATE_NEW_CONSOLE, except that the new console either has
    //    a hidden window or has no window at all.
    //
    printTestName("Test_CreateNoWindow");

    // XXX: Test that CREATE_NEW_CONSOLE + DETACHED_PROCESS ==> failure
    // XXX: Test that CREATE_NEW_CONSOLE + CREATE_NO_WINDOW ==> CREATE_NEW_CONSOLE
    // XXX: Test that DETACHED_PROCESS + CREATE_NO_WINDOW ==> DETACHED_PROCESS

    Worker p;
    // Open some handles to demonstrate the "clean slate" outcome.
    p.getStdin().dup(TRUE).setStdin();
    p.newBuffer(TRUE).setStderr().dup(TRUE).setStdout().activate();
    SpawnParams sp;
    sp.bInheritHandles = TRUE;
    sp.dwCreationFlags |= CREATE_NO_WINDOW;
    auto c = p.child(sp);
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

static void Test_NewConsole_Resets_Everything() {
    // Test that creating a new console properly resets everything.
    //
    // Tests:
    //  * CreateProcess - CREATE_NEW_CONSOLE
    //  * AllocConsole
    //
    printTestName("Test_NewConsole_Resets_Everything");
    Worker p;

    // Open some handles to demonstrate the "clean slate" outcome.
    auto orig = { p.getStdin(), p.getStdout(), p.getStderr() };
    p.getStdin().dup(TRUE).setStdin();
    p.newBuffer(TRUE).setStderr().dup(TRUE).setStdout().activate();
    for (auto &h : orig) {
        Handle::invent(h.value(), p).close();
    }

    // XXX: On Win8+, I doubt we can expect particular console handle values...
    // XXX: Actually, I'm not sure how much of this test is valid there at all...
    auto checkClean = [](Worker &proc) {
        proc.dumpConsoleHandles();
        CHECK(proc.getStdin().uvalue() == 0x3);
        CHECK(proc.getStdout().uvalue() == 0x7);
        CHECK(proc.getStderr().uvalue() == 0xb);
        auto handles = proc.scanForConsoleHandles();
        CHECK(handleValues(handles) == (std::vector<HANDLE> {
            proc.getStdin().value(),
            proc.getStdout().value(),
            proc.getStderr().value(),
        }));
        for (auto &h : handles) {
            CHECK(inheritable(h));
        }
    };

    // A child with a new console is reset to a blank slate.
    SpawnParams sp;
    sp.bInheritHandles = TRUE;
    sp.dwCreationFlags |= CREATE_NEW_CONSOLE;
    auto c = p.child(sp);
    checkClean(c);

    // Similarly, detaching and allocating a new console resets everything.
    p.detach();
    p.alloc();
    checkClean(p);
}

static void Test_DetachedProcess() {
    // A child with DETACHED_PROCESS has no console, and its standard handles
    // are set to 0 by default.
    printTestName("Test_DetachedProcess");
    Worker p;

    p.getStdin().dup(TRUE).setStdin();
    p.getStdout().dup(TRUE).setStdout();
    p.getStderr().dup(TRUE).setStderr();

    SpawnParams sp;
    sp.bInheritHandles = TRUE;
    sp.dwCreationFlags |= DETACHED_PROCESS;
    auto c = p.child(sp);

    CHECK(c.getStdin().uvalue() == 0);
    CHECK(c.getStdout().uvalue() == 0);
    CHECK(c.getStderr().uvalue() == 0);
    CHECK(c.scanForConsoleHandles().empty());
    CHECK(c.consoleWindow() == NULL);

    // XXX: What do GetConsoleCP and GetConsoleOutputCP do when no console is attached?

    // Verify that we have a blank slate even with an implicit console
    // creation.
    sp = SpawnParams();
    sp.bInheritHandles = TRUE;
    auto c2 = c.child(sp);
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
    printTestName("Test_Creation_bInheritHandles_Flag");
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
    SpawnParams spY; spY.bInheritHandles = FALSE;
    SpawnParams spN; spN.bInheritHandles = TRUE;
    auto cY = p.child(spY);
    auto cN = p.child(spN);
    CHECK(handleValues(cY.scanForConsoleHandles()) ==
          handleValues(cN.scanForConsoleHandles()));
}

static void Test_HandleAllocationOrder() {
    // When a new handle is created, it always assumes the lowest unused value.
    printTestName("Test_HandleAllocationOrder");
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
    printTestName("Test_InheritNothing");
    Worker p;
    auto conin = p.openConin();
    auto conout = p.openConout();
    p.getStdin().close();
    p.getStdout().close();
    p.getStderr().close();
    conin.setStdin();
    conout.setStdout().dup().setStderr();
    p.dumpConsoleHandles();

    SpawnParams sp;
    sp.bInheritHandles = TRUE;
    auto c = p.child(sp);
    // The child has no open console handles.
    CHECK(c.scanForConsoleHandles().empty());
    c.dumpConsoleHandles();
    // The standard handle values are inherited, even though they're invalid.
    CHECK(c.getStdin().value() == p.getStdin().value());
    CHECK(c.getStdout().value() == p.getStdout().value());
    CHECK(c.getStderr().value() == p.getStderr().value());
    // Verify a console is attached.
    CHECK(c.consoleWindow() != NULL);
    CHECK(c.openConin().value() != INVALID_HANDLE_VALUE);
    CHECK(c.openConout().value() != INVALID_HANDLE_VALUE);
    CHECK(c.newBuffer().value() != INVALID_HANDLE_VALUE);
}

// XXX: Does specifying a handle in STD_{...}_HANDLE or hStd{Input,...}
// influence whether it is inherited, in any situation?

static void Test_Detach_Does_Not_Change_Standard_Handles() {
    // Detaching the current console does not affect the standard handles.
    printTestName("Test_Detach_Does_Not_Change_Standard_Handles");
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
    printTestName("Test_Activate_Does_Not_Change_Standard_Handles");
    Worker p;
    auto out = p.getStdout();
    auto err = p.getStderr();
    p.newBuffer(TRUE).activate();
    CHECK(p.getStdout().value() == out.value());
    CHECK(p.getStderr().value() == err.value());
}

static void Test_AttachConsole_And_CreateProcess_Inheritance() {
    printTestName("Test_AttachConsole_And_CreateProcess_Inheritance");
    Worker p;

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

    SpawnParams sp;
    sp.bInheritHandles = TRUE;
    auto c = p.child(sp);

    auto c2 = c.child(sp);
    c2.detach();
    c2.attach(c);

    // The first child will have the same standard handles as the parent.
    CHECK(c.getStdin().value() == p.getStdin().value());
    CHECK(c.getStdout().value() == p.getStdout().value());
    CHECK(c.getStderr().value() == p.getStderr().value());

    // AttachConsole always sets the handles to (0x3, 0x7, 0xb) regardless of
    // handle validity.  In this case, c2 initially had non-default handles,
    // and it attached to a process that has and also initially had
    // non-default handles.  Nevertheless, the new standard handles are the
    // defaults.
    CHECK(c2.getStdin().uvalue() == 0x3);
    CHECK(c2.getStdout().uvalue() == 0x7);
    CHECK(c2.getStderr().uvalue() == 0xb);

    // The set of inheritable console handles in the two children exactly match
    // that of the parent.
    checkAttachHandleSet(c, p);
    checkAttachHandleSet(c2, p);
}

static void Test_Detach_Implicitly_Closes_Handles() {
    // After detaching, calling GetHandleInformation fails on previous console
    // handles.  Prior to Windows 8, this property applied to all console
    // handles; in Windows 8, it only applies to some handles, apparently.

    printTestName("Test_Detach_Implicitly_Closes_Handles");
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
            CHECK(inheritable(h));
        }
    }
}

static void Test_Conin_Versus_Conout() {
    // Ensure that APIs meant for the other kind of handle fail.
    printTestName("Test_Conin_Versus_Conout");
    Worker p;
    CHECK(!p.getStdin().tryScreenBufferInfo());
    CHECK(!p.getStdout().tryNumberOfConsoleInputEvents());
}

int main() {
    CHECK(false && "Just making sure the CHECK macro is working...");

    Test_IntrinsicInheritFlags();
    Test_CreateNoWindow();
    Test_NewConsole_Resets_Everything();
    Test_DetachedProcess();
    Test_Creation_bInheritHandles_Flag();
    Test_HandleAllocationOrder();
    Test_InheritNothing();
    Test_Detach_Does_Not_Change_Standard_Handles();
    Test_Activate_Does_Not_Change_Standard_Handles();
    Test_AttachConsole_And_CreateProcess_Inheritance();
    Test_Detach_Implicitly_Closes_Handles();
    Test_Conin_Versus_Conout();

    return 0;
}
