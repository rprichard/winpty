//
// 2015-09-25
// I measured these limits on the size of a single ReadConsoleOutputW call.
// The limit seems to more-or-less disppear with Windows 8, which is the first
// OS to stop using ALPCs for console I/O.  My guess is that the new I/O
// method does not use the 64KiB shared memory buffer that the ALPC method
// uses.
//
// I'm guessing the remaining difference between Windows 8/8.1 and Windows 10
// might be related to the 32-vs-64-bitness.
//
// Windows XP 32-bit VM ==> up to 13304 characters
//    13304x1 works, but 13305x1 fails instantly
// Windows 7 32-bit VM ==> between 16-17 thousand characters
//    16000x1 works, 17000x1 fails instantly
//    163x100 *crashes* conhost.exe but leaves VeryLargeRead.exe running
// Windows 8 32-bit VM ==> between 240-250 million characters
//    10000x24000 works, but 10000x25000 does not
// Windows 8.1 32-bit VM ==> between 240-250 million characters
//    10000x24000 works, but 10000x25000 does not
// Windows 10 64-bit VM ==> no limit (tested to 576 million characters)
//    24000x24000 works
//

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
