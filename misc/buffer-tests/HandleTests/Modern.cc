#include <TestCommon.h>

REGISTER(Test_AttachConsole_AllocConsole_StdHandles, isModernConio);
static void Test_AttachConsole_AllocConsole_StdHandles() {
    // Verify that AttachConsole does the right thing w.r.t. console handle
    // sets and standard handles.

    auto check = [](bool newConsole, bool useStdHandles, int nullIndex) {
        trace("checking: newConsole=%d useStdHandles=%d nullIndex=%d",
            newConsole, useStdHandles, nullIndex);
        Worker p;
        SpawnParams sp = useStdHandles
            ? SpawnParams { true, 0, stdHandles(p) }
            : SpawnParams { false, 0 };

        auto c = p.child(sp);
        auto pipe = newPipe(c, true);
        std::get<0>(pipe).setStdin();
        std::get<1>(pipe).setStdout().setStdout();

        if (nullIndex == 0) {
            Handle::invent(nullptr, c).setStdin();
        } else if (nullIndex == 1) {
            Handle::invent(nullptr, c).setStdout();
        } else if (nullIndex == 2) {
            Handle::invent(nullptr, c).setStderr();
        }

        auto origStdHandles = stdHandles(c);
        c.detach();
        CHECK(handleValues(stdHandles(c)) == handleValues(origStdHandles));

        if (newConsole) {
            c.alloc();
        } else {
            Worker other;
            c.attach(other);
        }

        if (useStdHandles) {
            auto curHandles = stdHandles(c);
            for (int i = 0; i < 3; ++i) {
                if (i != nullIndex) {
                    CHECK(curHandles[i].value() == origStdHandles[i].value());
                }
            }
            checkModernConsoleHandleInit(c,
                nullIndex == 0,
                nullIndex == 1,
                nullIndex == 2);
        } else {
            checkModernConsoleHandleInit(c, true, true, true);
        }
    };

    for (int i = -1; i < 3; ++i) {
        check(false, false, i);
        check(false, true, i);
        check(true, false, i);
        check(true, true, i);
    }
}
