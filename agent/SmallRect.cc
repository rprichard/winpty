#include "SmallRect.h"
#include <stdio.h>

std::string SmallRect::toString() const
{
    char ret[64];
    sprintf(ret, "(x=%d,y=%d,w=%d,h=%d)",
            Left, Top, width(), height());
    return std::string(ret);
}
