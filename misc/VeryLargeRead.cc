#include <windows.h>
#include <assert.h>
#include <vector>

#include "TestUtil.cc"
#include "../shared/DebugClient.cc"

int main(int argc, char *argv[]) {
    if (argc == 1) {
        startChildProcess(L"CHILD");
        return 0;
    }

    const HANDLE conout = GetStdHandle(STD_OUTPUT_HANDLE);
    const long long kWidth = 9000;
    const long long kHeight = 9000;

    setWindowPos(0, 0, 1, 1);
    setBufferSize(kWidth, kHeight);
    setWindowPos(0, 0, std::min(80LL, kWidth), std::min(50LL, kHeight));

    setCursorPos(0, 0);
    printf("A");
    fflush(stdout);
    setCursorPos(kWidth - 2, kHeight - 1);
    printf("B");
    fflush(stdout);

    trace("sizeof(CHAR_INFO) = %d", (int)sizeof(CHAR_INFO));

    trace("Allocating buffer...");
    CHAR_INFO *buffer = new CHAR_INFO[kWidth * kHeight];
    assert(buffer != NULL);
    memset(&buffer[0], 0, sizeof(CHAR_INFO));
    memset(&buffer[kWidth * kHeight - 2], 0, sizeof(CHAR_INFO));

    COORD bufSize = { kWidth, kHeight };
    COORD bufCoord = { 0, 0 };
    SMALL_RECT readRegion = { 0, 0, kWidth - 1, kHeight - 1 };
    trace("ReadConsoleOutputW: calling...");
    BOOL success = ReadConsoleOutputW(conout, buffer, bufSize, bufCoord, &readRegion);
    trace("ReadConsoleOutputW: success=%d", success);

    assert(buffer[0].Char.UnicodeChar == L'A');
    assert(buffer[kWidth * kHeight - 2].Char.UnicodeChar == L'B');
    trace("Top-left and bottom-right characters read successfully!");

    Sleep(30000);

    delete [] buffer;
    return 0;
}
