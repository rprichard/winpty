#ifndef SMALLRECT_H
#define SMALLRECT_H

#include <windows.h>
#include "Coord.h"
#include <algorithm>
#include <string>

struct SmallRect : SMALL_RECT
{
    SmallRect()
    {
        Left = Right = Top = Bottom = 0;
    }

    SmallRect(SHORT x, SHORT y, SHORT width, SHORT height)
    {
        Left = x;
        Top = y;
        Right = x + width - 1;
        Bottom = y + height - 1;
    }

    SmallRect(const COORD &topLeft, const COORD &size)
    {
        Left = topLeft.X;
        Top = topLeft.Y;
        Right = Left + size.X - 1;
        Bottom = Top + size.Y - 1;
    }

    SmallRect(const SMALL_RECT &other)
    {
        *(SMALL_RECT*)this = other;
    }

    SmallRect(const SmallRect &other)
    {
        *(SMALL_RECT*)this = *(const SMALL_RECT*)&other;
    }

    SmallRect &operator=(const SmallRect &other)
    {
        *(SMALL_RECT*)this = *(const SMALL_RECT*)&other;
        return *this;
    }

    bool contains(const SmallRect &other) const
    {
        return other.Left >= Left &&
               other.Right <= Right &&
               other.Top >= Top &&
               other.Bottom <= Bottom;
    }

    SmallRect intersected(const SmallRect &other) const
    {
        return SmallRect(std::max(Left, other.Left),
                         std::min(Right, other.Right),
                         std::max(Top, other.Top),
                         std::min(Bottom, other.Bottom));
    }

    SHORT top() const               { return Top;                       }
    SHORT left() const              { return Left;                      }
    SHORT width() const             { return Right - Left + 1;          }
    SHORT height() const            { return Bottom - Top + 1;          }
    void setTop(SHORT top)          { Top = top;                        }
    void setLeft(SHORT left)        { Left = left;                      }
    void setWidth(SHORT width)      { Right = Left + width - 1;         }
    void setHeight(SHORT height)    { Bottom = Top + height - 1;        }
    Coord size() const              { return Coord(width(), height());  }

    bool operator==(const SmallRect &other) const
    {
        return Left == other.Left &&
               Right == other.Right &&
               Top == other.Top &&
               Bottom == other.Bottom;
    }

    bool operator!=(const SmallRect &other) const
    {
        return !(*this == other);
    }

    std::string toString() const;
};

#endif // SMALLRECT_H
