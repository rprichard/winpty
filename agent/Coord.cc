#include "Coord.h"
#include <stdio.h>

std::string Coord::toString() const
{
    char ret[32];
    sprintf(ret, "(%d,%d)", X, Y);
    return std::string(ret);
}
