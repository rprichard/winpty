#include <windows.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    system("cls");

    // Write character.
    wchar_t ch = 0x754C; // U+754C (CJK UNIFIED IDEOGRAPH-754C)
    DWORD actual = 0;
    BOOL ret = WriteConsoleW(
        GetStdHandle(STD_OUTPUT_HANDLE),
        &ch, 1, &actual, NULL);
    assert(ret && actual == 1);

    // Read it back.
    CHAR_INFO data[2];
    COORD bufSize = {2, 1};
    COORD bufCoord = {0, 0};
    SMALL_RECT topLeft = {0, 0, 1, 0};
    ret = ReadConsoleOutputW(
            GetStdHandle(STD_OUTPUT_HANDLE), data, bufSize, bufCoord, &topLeft);
    assert(ret);

    printf("\n");
    printf("CHAR: 0x%04x\n", data[0].Char.UnicodeChar);
    printf("CHAR: 0x%04x\n", data[1].Char.UnicodeChar);
    return 0;
}
