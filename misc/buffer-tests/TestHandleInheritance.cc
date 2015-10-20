#include <TestCommon.h>

int main() {
    SpawnParams sp;
    sp.bInheritHandles = FALSE;

    Worker p;
    p.getStdout().write("<-- origBuffer -->");

    auto b1 = p.newBuffer(FALSE);
    auto b2 = p.newBuffer(TRUE);
    auto b3 = b1.dup(FALSE);
    auto b5 = b2.dup(FALSE);
    auto b4 = b1.dup(TRUE);
    auto b6 = b2.dup(TRUE);
    p.dumpScreenBuffers();

    Sleep(300000);
}
