#include <TestCommon.h>

void runCommonTests();
void runTraditionalTests();
void runModernTests();

int main() {
    runCommonTests();
    if (isAtLeastWin8()) {
        runModernTests();
    } else {
        runTraditionalTests();
    }
    return 0;
}
