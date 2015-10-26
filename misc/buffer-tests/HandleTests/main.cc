#include <TestCommon.h>

int main() {
    for (DWORD flags : {CREATE_NEW_CONSOLE, CREATE_NO_WINDOW}) {
        if (flags == CREATE_NEW_CONSOLE) {
            printTestName("Using CREATE_NEW_CONSOLE as default creation mode");
        } else {
            printTestName("Using CREATE_NO_WINDOW as default creation mode");
        }
        Worker::setDefaultCreationFlags(flags);
        for (auto &test : registeredTests()) {
            std::string name;
            bool (*cond)();
            void (*func)();
            std::tie(name, cond, func) = test;
            if (cond()) {
                printTestName(name);
                func();
            }
        }
    }
    return 0;
}
