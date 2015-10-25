#include <TestCommon.h>

void runCommonTests();
void runCommonTests_CreateProcess();
void runTraditionalTests();
void runModernTests();

int main() {
    runCommonTests();
    runCommonTests_CreateProcess();
    if (isAtLeastWin8()) {
        runModernTests();
    } else {
        runTraditionalTests();
    }
    return 0;
}
