#include "Util.h"

#include "HookAssert.h"
#include "UnicodeConversions.h"

std::string pathDirName(const std::string &path)
{
    std::string::size_type pos = path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return std::string();
    } else {
        return path.substr(0, pos);
    }
}

// Wrapper for GetModuleFileNameW.  Returns a UTF-8 string.  Aborts on error.
std::string getModuleFileName(HMODULE module)
{
    const DWORD size = 4096;
    wchar_t filename[size];
    DWORD actual = GetModuleFileNameW(module, filename, size);
    ASSERT(actual > 0 && actual < size);
    return narrowString(filename);
}
