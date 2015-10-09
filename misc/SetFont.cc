#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include "TestUtil.cc"
#include "../shared/DebugClient.cc"

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

// Attempt to set the console font to the given facename and pixel size.
// These APIs should exist on Vista and up.
static void setConsoleFont(const wchar_t *faceName, int pixelSize)
{
    CONSOLE_FONT_INFOEX fontex = {0};
    fontex.cbSize = sizeof(fontex);
    fontex.FontWeight = 400;
    fontex.dwFontSize.Y = pixelSize;
    wcsncpy(fontex.FaceName, faceName, COUNT_OF(fontex.FaceName));
    fontex.nFont = 34;
    BOOL ret = SetCurrentConsoleFontEx(
        GetStdHandle(STD_OUTPUT_HANDLE),
        FALSE,
        &fontex);
    printf("SetCurrentConsoleFontEx returned %d\n", ret);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("Usage:\n");
        printf("  SetFont <index>\n");
        printf("  SetFont options\n");
        printf("\n");
        printf("Options for SetCurrentConsoleFontEx:\n");
        printf("  -idx INDEX\n");
        printf("  -w WIDTH\n");
        printf("  -h HEIGHT\n");
        printf("  -weight (normal|bold|NNN)\n");
        printf("  -face FACENAME\n");
        printf("  -tt\n");
        printf("  -vec\n");
        printf("  -vp\n");
        printf("  -dev\n");
        printf("  -roman\n");
        printf("  -swiss\n");
        printf("  -modern\n");
        printf("  -script\n");
        printf("  -decorative\n");
        return 0;
    }

    if (isdigit(argv[1][0])) {
        int index = atoi(argv[1]);
        HMODULE kernel32 = LoadLibraryW(L"kernel32.dll");
        FARPROC proc = GetProcAddress(kernel32, "SetConsoleFont");
        if (proc == NULL) {
            printf("Couldn't get address of SetConsoleFont\n");
        } else {
            const HANDLE conout = GetStdHandle(STD_OUTPUT_HANDLE);
            BOOL ret = reinterpret_cast<BOOL WINAPI(*)(HANDLE, DWORD)>(proc)(
                    conout, index);
            printf("SetFont returned %d\n", ret);
        }
        return 0;
    }

    CONSOLE_FONT_INFOEX fontex = {0};
    fontex.cbSize = sizeof(fontex);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (i + 1 < argc) {
            std::string next = argv[i + 1];
            if (arg == "-idx") {
                fontex.nFont = atoi(next.c_str());
                ++i; continue;
            } else if (arg == "-w") {
                fontex.dwFontSize.X = atoi(next.c_str());
                ++i; continue;
            } else if (arg == "-h") {
                fontex.dwFontSize.Y = atoi(next.c_str());
                ++i; continue;
            } else if (arg == "-weight") {
                if (next == "normal") {
                    fontex.FontWeight = 400;
                } else if (next == "bold") {
                    fontex.FontWeight = 700;
                } else {
                    fontex.FontWeight = atoi(next.c_str());
                }
                ++i; continue;
            } else if (arg == "-face") {
                memset(&fontex.FaceName, 0, sizeof(fontex.FaceName));
                mbstowcs(fontex.FaceName, next.c_str(), COUNT_OF(fontex.FaceName) - 1);
                fontex.FaceName[COUNT_OF(fontex.FaceName) - 1] = L'\0';
                ++i; continue;
            }
        }
        if (arg == "-tt") {
            fontex.FontFamily |= TMPF_TRUETYPE;
        } else if (arg == "-vec") {
            fontex.FontFamily |= TMPF_VECTOR;
        } else if (arg == "-vp") {
            // Setting the TMPF_FIXED_PITCH bit actually indicates variable
            // pitch.
            fontex.FontFamily |= TMPF_FIXED_PITCH;
        } else if (arg == "-dev") {
            fontex.FontFamily |= TMPF_DEVICE;
        } else if (arg == "-roman") {
            fontex.FontFamily = (fontex.FontFamily & ~0xF0) | FF_ROMAN;
        } else if (arg == "-swiss") {
            fontex.FontFamily = (fontex.FontFamily & ~0xF0) | FF_SWISS;
        } else if (arg == "-modern") {
            fontex.FontFamily = (fontex.FontFamily & ~0xF0) | FF_MODERN;
        } else if (arg == "-script") {
            fontex.FontFamily = (fontex.FontFamily & ~0xF0) | FF_SCRIPT;
        } else if (arg == "-decorative") {
            fontex.FontFamily = (fontex.FontFamily & ~0xF0) | FF_DECORATIVE;
        } else if (arg == "-face-gothic") {
            // ＭＳ ゴシック
            const wchar_t gothicFace[] = {
                0xFF2D, 0xFF33, 0x20, 0x30B4, 0x30B7, 0x30C3, 0x30AF, 0x0
            };
            memset(&fontex.FaceName, 0, sizeof(fontex.FaceName));
            wcscpy(fontex.FaceName, gothicFace);
        } else {
            printf("Unrecognized argument: %s\n", arg.c_str());
            exit(1);
        }
    }

    cprintf(L"Setting to: nFont=%u dwFontSize=(%d,%d) "
        L"FontFamily=0x%x FontWeight=%u "
        L"FaceName=\"%ls\"\n",
        static_cast<unsigned>(fontex.nFont),
        fontex.dwFontSize.X, fontex.dwFontSize.Y,
        fontex.FontFamily, fontex.FontWeight,
        fontex.FaceName);

    BOOL ret = SetCurrentConsoleFontEx(
        GetStdHandle(STD_OUTPUT_HANDLE),
        FALSE,
        &fontex);
    printf("SetCurrentConsoleFontEx returned %d\n", ret);

    return 0;
}
