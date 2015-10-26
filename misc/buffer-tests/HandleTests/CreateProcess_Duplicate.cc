#include <TestCommon.h>

// If CreateProcess is called with these parameters:
//  - bInheritHandles=FALSE
//  - STARTF_USESTDHANDLES is not specified
//  - the "CreationConsoleMode" is Inherit (see console-handles.md)
// then Windows duplicates each of STDIN/STDOUT/STDERR to the child.
//
// There are variations between OS releases, especially with regards to
// how console handles work.

namespace {

template <typename T>
void Test_CreateProcess_Duplicate_Impl(T makeChild) {
    printTestName(__FUNCTION__);
    {
        // Base case: a non-inheritable pipe is still inherited.
        Worker p;
        auto pipe = newPipe(p, false);
        auto wh = std::get<1>(pipe).setStdin().setStdout().setStderr();
        auto c = makeChild(p, { false });
        {
            ObjectSnap snap;
            CHECK(snap.eq({ c.getStdin(), c.getStdout(), c.getStderr(), wh }));
        }
        // CreateProcess makes separate handles for stdin/stdout/stderr.
        CHECK(c.getStdin().value() != c.getStdout().value());
        CHECK(c.getStdout().value() != c.getStderr().value());
        CHECK(c.getStdin().value() != c.getStderr().value());
        // Calling FreeConsole in the child does not free the duplicated
        // handles.
        c.detach();
        {
            ObjectSnap snap;
            CHECK(snap.eq({ c.getStdin(), c.getStdout(), c.getStderr(), wh }));
        }
    }
    {
        // Bogus values are transformed into zero.
        Worker p;
        Handle::invent(0x10000ull, p).setStdin().setStdout();
        Handle::invent(0x0ull, p).setStderr();
        auto c = makeChild(p, { false });
        CHECK(handleInts(stdHandles(c)) == (std::vector<uint64_t> {0,0,0}));
    }

    if (isAtLeastWin8()) {
        // On Windows 8 and up, if a standard handle we duplicate just happens
        // to be a console handle, that isn't sufficient reason for FreeConsole
        // to close it.
        Worker p;
        auto c = makeChild(p, { false });
        auto ph = stdHandles(p);
        auto ch = stdHandles(c);
        auto check = [&]() {
            ObjectSnap snap;
            for (int i = 0; i < 3; ++i) {
                CHECK(snap.eq(ph[i], ch[i]));
                CHECK(ph[i].tryFlags() && ch[i].tryFlags());
                CHECK_EQ(ph[i].tryFlags() && ph[i].inheritable(),
                         ch[i].tryFlags() && ch[i].inheritable());
            }
        };
        check();
        c.detach();
        check();
    }

    {
        // Traditional console-like values are passed through as-is,
        // up to 0x0FFFFFFFull.
        Worker p;
        Handle::invent(0x0FFFFFFFull, p).setStdin();
        Handle::invent(0x10000003ull, p).setStdout();
        Handle::invent(0x00000003ull, p).setStderr();
        auto c = makeChild(p, { false });
        if (isAtLeastWin8()) {
            // These values are invalid on Windows 8 and turned into NULL.
            CHECK(handleInts(stdHandles(c)) ==
                (std::vector<uint64_t> { 0, 0, 0 }));
        } else {
            CHECK(handleInts(stdHandles(c)) ==
                (std::vector<uint64_t> { 0x0FFFFFFFull, 0, 3 }));
        }
    }

    {
        // Test setting STDIN/STDOUT/STDERR to non-inheritable console handles.
        //
        // Handle duplication does not apply to traditional console handles,
        // and a console handle is inherited if and only if it is inheritable.
        //
        // On new releases, this will Just Work.
        //
        Worker p;
        p.getStdout().setFirstChar('A');
        p.openConin(false).setStdin();
        p.newBuffer(false, 'B').setStdout().setStderr();
        auto c = makeChild(p, { false });

        if (!isAtLeastWin8()) {
            CHECK(handleValues(stdHandles(p)) == handleValues(stdHandles(c)));
            CHECK(!c.getStdin().tryFlags());
            CHECK(!c.getStdout().tryFlags());
            CHECK(!c.getStderr().tryFlags());
        } else {
            // In Win8, a console handle works like all other handles.
            CHECK_EQ(c.getStdout().firstChar(), 'B');
            ObjectSnap snap;
            CHECK(snap.eq({ p.getStdout(), p.getStderr(),
                            c.getStdout(), c.getStderr() }));
            CHECK(!c.getStdout().inheritable());
            CHECK(!c.getStderr().inheritable());
        }
    }
}

} // anonymous namespace

REGISTER(Test_CreateProcess_Duplicate, always);
static void Test_CreateProcess_Duplicate() {
    Test_CreateProcess_Duplicate_Impl([](Worker &p, SpawnParams sp) {
        return p.child(sp);
    });
    if (isModernConio()) {
        // With modern console I/O, calling CreateProcess with these
        // parameters also duplicates standard handles:
        //  - bInheritHandles=TRUE
        //  - STARTF_USESTDHANDLES not specified
        //  - an inherit list (PROC_THREAD_ATTRIBUTE_HANDLE_LIST) is specified
        Test_CreateProcess_Duplicate_Impl([](Worker &p, SpawnParams sp) {
            return childWithDummyInheritList(p, sp, false);
        });
        Test_CreateProcess_Duplicate_Impl([](Worker &p, SpawnParams sp) {
            return childWithDummyInheritList(p, sp, true);
        });
    }
}
