#include <windows.h>
#include <stdlib.h>

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
    SetCurrentConsoleFontEx(
        GetStdHandle(STD_OUTPUT_HANDLE),
        FALSE,
        &fontex);
}

int main(int argc, char *argv[]) {
    wchar_t faceName[256];
    mbstowcs(faceName, argv[1], COUNT_OF(faceName) - 1);
    faceName[COUNT_OF(faceName) - 1] = L'\0';
    setConsoleFont(faceName, atoi(argv[2]));
    return 0;
}
