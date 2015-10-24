#pragma once

#include <windows.h>

#include <tuple>

#include <WinptyAssert.h>

inline std::tuple<int, int> osversion() {
    OSVERSIONINFO info = { sizeof(info), 0 };
    ASSERT(GetVersionEx(&info));
    return std::make_tuple(info.dwMajorVersion, info.dwMinorVersion);
}

inline bool isWin7() {
    return osversion() == std::make_tuple(6, 1);
}

inline bool isAtLeastVista() {
    return osversion() >= std::make_tuple(6, 0);
}

inline bool isAtLeastWin7() {
    return osversion() >= std::make_tuple(6, 1);
}

inline bool isAtLeastWin8() {
    return osversion() >= std::make_tuple(6, 2);
}

inline bool isAtLeastWin8_1() {
    return osversion() >= std::make_tuple(6, 3);
}
