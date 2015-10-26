#include <TestCommon.h>

//
// Test CreateProcess when called with these parameters:
//  - STARTF_USESTDHANDLES is not specified
//  - bInheritHandles=FALSE or bInheritHandles=TRUE
//  - CreationConsoleMode=NewConsole
//

template <typename T>
static void extend(std::vector<T> &base, const std::vector<T> &to_add) {
    base.insert(base.end(), to_add.begin(), to_add.end());
}

REGISTER(Test_CreateProcess_NewConsole, always);
static void Test_CreateProcess_NewConsole() {
    auto check = [](Worker &p, bool inheritHandles) {
        auto c = p.child({ inheritHandles, CREATE_NEW_CONSOLE });
        if (isTraditionalConio()) {
            checkInitConsoleHandleSet(c);
        } else {
            checkModernConsoleHandleInit(c, true, true, true);
        }
        return c;
    };
    {
        Worker p;
        check(p, true);
        check(p, false);
    }
    {
        Worker p;
        p.openConin(false).setStdin();
        p.newBuffer(false).setStdout().dup(true).setStderr();
        check(p, true);
        check(p, false);
    }

    if (isModernConio()) {
        // The default Unbound console handles should be inheritable, so with
        // bInheritHandles=TRUE, the child process should have six console
        // handles, all usable.
        Worker p;
        auto c = check(p, true);

        std::vector<uint64_t> expected;
        extend(expected, handleInts(stdHandles(p)));
        extend(expected, handleInts(stdHandles(c)));
        std::sort(expected.begin(), expected.end());

        auto correct = handleInts(c.scanForConsoleHandles());
        std::sort(correct.begin(), correct.end());

        CHECK(expected == correct);
    }
}
