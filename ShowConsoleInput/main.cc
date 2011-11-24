#include <QtCore/QCoreApplication>
#include <windows.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    while (true) {
        DWORD count;
        INPUT_RECORD ir;
        if (!ReadConsoleInput(hStdin, &ir, 1, &count)) {
            printf("ReadConsoleInput failed\n");
            return 1;
        }
        if (ir.EventType == KEY_EVENT) {
            const KEY_EVENT_RECORD &ker = ir.Event.KeyEvent;
            printf("%s", ker.bKeyDown ? "dn" : "up");
            printf(" ch=");
            if (isprint(ker.uChar.AsciiChar))
                printf("'%c'", ker.uChar.AsciiChar);
            printf("%d", ker.uChar.AsciiChar);
            printf(" ctrl=%d", (int)ker.dwControlKeyState);
            printf(" vk=%d", ker.wVirtualKeyCode);
            printf(" scan=%d", ker.wVirtualScanCode);
            printf(" repeat=%d", ker.wRepeatCount);
            printf("\n");
        }
    }
}
