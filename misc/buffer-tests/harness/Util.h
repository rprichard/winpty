#pragma once

#include <windows.h>

#include <string>
#include <vector>

std::string pathDirName(const std::string &path);
std::string getModuleFileName(HMODULE module);
