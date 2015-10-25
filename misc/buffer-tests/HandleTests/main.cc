#include <TestCommon.h>

int main() {
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
    return 0;
}
