// This file is included into test programs using #include

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <vector>

#include "../shared/DebugClient.h"

// Launch this test program again, in a new console that we will destroy.
static void startChildProcess(const wchar_t *args) {
    wchar_t program[1024];
    wchar_t cmdline[1024];
    GetModuleFileNameW(NULL, program, 1024);
    swprintf(cmdline, L"\"%ls\" %ls", program, args);

    STARTUPINFOW sui;
    PROCESS_INFORMATION pi;
    memset(&sui, 0, sizeof(sui));
    memset(&pi, 0, sizeof(pi));
    sui.cb = sizeof(sui);

    CreateProcessW(program, cmdline,
                   NULL, NULL,
                   /*bInheritHandles=*/FALSE,
                   /*dwCreationFlags=*/CREATE_NEW_CONSOLE,
                   NULL, NULL,
                   &sui, &pi);
}

static void setBufferSize(HANDLE conout, int x, int y) {
    COORD size = { x, y };
    BOOL success = SetConsoleScreenBufferSize(conout, size);
    trace("setBufferSize: (%d,%d), result=%d", x, y, success);
}

static void setWindowPos(HANDLE conout, int x, int y, int w, int h) {
    SMALL_RECT r = { x, y, x + w - 1, y + h - 1 };
    BOOL success = SetConsoleWindowInfo(conout, /*bAbsolute=*/TRUE, &r);
    trace("setWindowPos: (%d,%d,%d,%d), result=%d", x, y, w, h, success);
}

static void setCursorPos(HANDLE conout, int x, int y) {
    COORD coord = { x, y };
    SetConsoleCursorPosition(conout, coord);
}

static void setBufferSize(int x, int y) {
    setBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), x, y);
}

static void setWindowPos(int x, int y, int w, int h) {
    setWindowPos(GetStdHandle(STD_OUTPUT_HANDLE), x, y, w, h);
}

static void setCursorPos(int x, int y) {
    setCursorPos(GetStdHandle(STD_OUTPUT_HANDLE), x, y);
}

static void countDown(int sec) {
    for (int i = sec; i > 0; --i) {
        printf("%d.. ", i);
        fflush(stdout);
        Sleep(1000);
    }
    printf("\n");
}

static void writeBox(int x, int y, int w, int h, char ch, int attributes=7) {
    CHAR_INFO info = { 0 };
    info.Char.AsciiChar = ch;
    info.Attributes = attributes;
    std::vector<CHAR_INFO> buf(w * h, info);
    HANDLE conout = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD bufSize = { w, h };
    COORD bufCoord = { 0, 0 };
    SMALL_RECT writeRegion = { x, y, x + w - 1, y + h - 1 };
    WriteConsoleOutputA(conout, buf.data(), bufSize, bufCoord, &writeRegion);
}

static void setChar(int x, int y, char ch, int attributes=7) {
    writeBox(x, y, 1, 1, ch, attributes);
}

static void fillChar(int x, int y, int repeat, char ch) {
    COORD coord = { x, y };
    DWORD actual = 0;
    FillConsoleOutputCharacterA(
        GetStdHandle(STD_OUTPUT_HANDLE),
        ch, repeat, coord, &actual);
}

static void repeatChar(int count, char ch) {
    for (int i = 0; i < count; ++i) {
        putchar(ch);
    }
    fflush(stdout);
}