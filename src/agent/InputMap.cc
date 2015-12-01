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

#include "InputMap.h"

#include <stdlib.h>
#include <string.h>

InputMap::InputMap() : m_key(NULL), m_children(NULL) {
}

InputMap::~InputMap() {
    delete m_key;
    if (m_children != NULL) {
        for (int i = 0; i < 256; ++i) {
            delete (*m_children)[i];
        }
    }
    delete [] m_children;
}

void InputMap::set(const char *encoding, const Key &key) {
    unsigned char ch = encoding[0];
    if (ch == '\0') {
        setKey(key);
    } else {
        getOrCreateChild(ch)->set(encoding + 1, key);
    }
}

void InputMap::setKey(const Key &key) {
    delete m_key;
    m_key = new Key(key);
}

InputMap *InputMap::getOrCreateChild(unsigned char ch) {
    if (m_children == NULL) {
        m_children = reinterpret_cast<InputMap*(*)[256]>(new InputMap*[256]);
        memset(m_children, 0, sizeof(InputMap*) * 256);
    }
    if ((*m_children)[ch] == NULL) {
        (*m_children)[ch] = new InputMap;
    }
    return (*m_children)[ch];
}
