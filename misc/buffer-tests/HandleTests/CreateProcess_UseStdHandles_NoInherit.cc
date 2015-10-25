#include <TestCommon.h>

template <typename T>
void checkVariousInputs(T check) {
    {
        // Specify the original std values.  Starting with Windows 8, this code
        // produces NULL child standard handles.  It used to produce valid
        // child standard handles.  (i.e. It used to work, but no longer does.)
        Worker p;
        check(p, true, stdHandles(p));
    }
    {
        // Completely invalid handles.
        Worker p;
        check(p, false, {
            Handle::invent(nullptr, p),
            Handle::invent(0x10000ull, p),
            Handle::invent(0xdeadbeefull, p),
        });
        check(p, false, {
            Handle::invent(INVALID_HANDLE_VALUE, p),
            Handle::invent(nullptr, p),
            Handle::invent(nullptr, p),
        });
    }
    {
        // Try a non-inheritable pipe.
        Worker p;
        auto pipe = newPipe(p, false);
        check(p, true, {
            std::get<0>(pipe),
            std::get<1>(pipe),
            std::get<1>(pipe),
        });
    }
    {
        // Try an inheritable pipe.
        Worker p;
        auto pipe = newPipe(p, true);
        check(p, true, {
            std::get<0>(pipe),
            std::get<1>(pipe),
            std::get<1>(pipe),
        });
    }
}

//
// Test CreateProcess when called with these parameters:
//  - bInheritHandles=FALSE
//  - STARTF_USESTDHANDLES is specified
//
// Before Windows 8, the child process has the same standard handles as the
// parent, without exception, and they might not be valid.
//
// As of Windows 8, the behavior depends upon the CreationConsole Mode:
//  - If it is Inherit, then the new standard handles are NULL.
//  - If it is NewConsole or NewConsoleWindow, then the child has three new
//    console handles, which FreeConsole will close.
//

//
// Part 1: CreationConsoleMode is Inherit
//
REGISTER(Test_CreateProcess_UseStdHandles_NoInherit_InheritConsoleMode, always);
static void Test_CreateProcess_UseStdHandles_NoInherit_InheritConsoleMode() {
    checkVariousInputs([](Worker &p,
                    bool validHandles,
                    std::vector<Handle> newHandles) {
        ASSERT(newHandles.size() == 3);
        auto c = p.child({false, 0, newHandles});
        if (isTraditionalConio()) {
            CHECK(handleValues(stdHandles(c)) == handleValues(newHandles));
            checkInitConsoleHandleSet(c, p);
            // The child handles have the same values as the parent.  If the
            // parent handles are valid kernel handles, then the child handles
            // are guaranteed not to reference the same object.
            auto childHandles = stdHandles(c);
            ObjectSnap snap;
            for (int i = 0; i < 3; ++i) {
                CHECK(!validHandles ||
                      newHandles[i].isTraditionalConsole() ||
                      !snap.eq(newHandles[i], childHandles[i]));
            }
        } else {
            CHECK(handleInts(stdHandles(c)) == (std::vector<uint64_t> {0,0,0}));
        }
    });
}

//
// Part 2: CreationConsoleMode is NewConsole
//

REGISTER(Test_CreateProcess_UseStdHandles_NoInherit_NewConsoleMode, always);
static void Test_CreateProcess_UseStdHandles_NoInherit_NewConsoleMode() {
    checkVariousInputs([](Worker &p,
                          bool validHandles,
                          std::vector<Handle> newHandles) {
        { auto h = p.openConout(); h.setFirstChar('P'); h.close(); }
        auto c = p.child({false, CREATE_NEW_CONSOLE, newHandles});
        { auto h = c.openConout(); CHECK_EQ(h.firstChar(), ' '); h.close(); }
        if (isTraditionalConio()) {
            CHECK(handleValues(stdHandles(c)) == handleValues(newHandles));
            checkInitConsoleHandleSet(c);
            // The child handles have the same values as the parent.  If the
            // parent handles are valid kernel handles, then the child handles
            // are guaranteed not to reference the same object.
            auto childHandles = stdHandles(c);
            ObjectSnap snap;
            for (int i = 0; i < 3; ++i) {
                CHECK(!validHandles ||
                      newHandles[i].isTraditionalConsole() ||
                      !snap.eq(newHandles[i], childHandles[i]));
            }
        } else {
            // Windows 8 acts exactly as if STARTF_USESTDHANDLES hadn't been
            // specified.
            //
            // There are three new handles to two new Unbound console objects.
            ObjectSnap snap;
            CHECK(isUsableConsoleInputHandle(c.getStdin()));
            CHECK(isUsableConsoleOutputHandle(c.getStdout()));
            CHECK(isUsableConsoleOutputHandle(c.getStderr()));
            CHECK(c.getStdout().value() != c.getStderr().value());
            CHECK(snap.eq(c.getStdout(), c.getStderr()));
            CHECK(isUnboundConsoleObject(c.getStdin()));
            CHECK(isUnboundConsoleObject(c.getStdout()));
            CHECK(isUnboundConsoleObject(c.getStderr()));

            // Aside: calling FreeConsole closes the three handles it opened.
            c.detach();
            CHECK(!c.getStdin().tryFlags());
            CHECK(!c.getStdout().tryFlags());
            CHECK(!c.getStderr().tryFlags());
        }
    });
}
