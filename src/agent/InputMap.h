// Copyright (c) 2011-2015 Ryan Prichard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef INPUT_MAP_H
#define INPUT_MAP_H

#include <stdlib.h>

#include <string>

#include "../shared/WinptyAssert.h"

class InputMap {
public:
    struct Key {
        int virtualKey;
        int unicodeChar;
        int keyState;

        std::string toString();
    };

    InputMap();
    ~InputMap();
    void set(const char *encoding, const Key &key);
    void setKey(const Key &key);
    const Key *getKey() const { return m_key; }
    bool hasChildren() const { return m_children != NULL; }
    InputMap *getChild(unsigned char ch) {
        return m_children != NULL ? (*m_children)[ch] : NULL;
    }
    InputMap *getOrCreateChild(unsigned char ch);

private:
    const Key *m_key;
    InputMap *(*m_children)[256];
};

#endif // INPUT_MAP_H
