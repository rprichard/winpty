#ifndef COORD_H
#define COORD_H

#include <windows.h>
#include <string>

struct Coord : COORD {
    Coord()
    {
        X = 0;
        Y = 0;
    }

    Coord(SHORT x, SHORT y)
    {
        X = x;
        Y = y;
    }

    Coord(COORD other)
    {
        *(COORD*)this = other;
    }

    Coord(const Coord &other)
    {
        *(COORD*)this = *(const COORD*)&other;
    }

    Coord &operator=(const Coord &other)
    {
        *(COORD*)this = *(const COORD*)&other;
        return *this;
    }

    bool operator==(const Coord &other) const
    {
        return X == other.X && Y == other.Y;
    }

    bool operator!=(const Coord &other) const
    {
        return !(*this == other);
    }

    Coord operator+(const Coord &other) const
    {
        return Coord(X + other.X, Y + other.Y);
    }

    bool isEmpty() const
    {
        return X <= 0 || Y <= 0;
    }

    std::string toString() const;
};

#endif // COORD_H
