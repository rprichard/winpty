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

#ifndef BUFFER_H
#define BUFFER_H

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <utility>
#include <vector>

#if !defined(__GNUC__) || defined(__EXCEPTIONS)
#define WINPTY_COMPILER_HAS_EXCEPTIONS 1
#else
#define WINPTY_COMPILER_HAS_EXCEPTIONS 0
#endif

#if WINPTY_COMPILER_HAS_EXCEPTIONS
#include <exception>
#endif

class WriteBuffer {
private:
    std::vector<char> m_buf;

public:
    WriteBuffer() {}

    template <typename T> void putRawValue(const T &t) {
        putRawData(&t, sizeof(t));
    }
    template <typename T> void replaceRawValue(size_t pos, const T &t) {
        replaceRawData(pos, &t, sizeof(t));
    }

    void putRawData(const void *data, size_t len);
    void putRawInt32(int32_t i)                 { putRawValue(i); }
    void replaceRawData(size_t pos, const void *data, size_t len);
    void replaceRawInt32(size_t pos, int32_t i) { replaceRawValue(pos, i); }
    void putInt(int i)                          { return putInt32(i); }
    void putInt32(int32_t i);
    void putInt64(int64_t i);
    void putWString(const wchar_t *str, size_t len);
    void putWString(const wchar_t *str)         { putWString(str, wcslen(str)); }
    void putWString(const std::wstring &str)    { putWString(str.data(), str.size()); }
    std::vector<char> &buf()                    { return m_buf; }

    // MSVC 2013 does not generate these automatically, so help it out.
    WriteBuffer(WriteBuffer &&other) : m_buf(std::move(other.m_buf)) {}
    WriteBuffer &operator=(WriteBuffer &&other) {
        m_buf = std::move(other.m_buf);
        return *this;
    }
};

class ReadBuffer {
public:
#if WINPTY_COMPILER_HAS_EXCEPTIONS
    class DecodeError : public std::exception {};
#endif

    enum ExceptionMode { Throw, NoThrow };

private:
    std::vector<char> m_buf;
    size_t m_off = 0;
    ExceptionMode m_exceptMode;

public:
    ReadBuffer(std::vector<char> &&buf, ExceptionMode exceptMode)
            : m_buf(std::move(buf)), m_exceptMode(exceptMode) {
        assert(WINPTY_COMPILER_HAS_EXCEPTIONS || exceptMode == NoThrow);
    }

    template <typename T> T getRawValue() {
        T ret = {};
        getRawData(&ret, sizeof(ret));
        return ret;
    }

    void getRawData(void *data, size_t len);
    int32_t getRawInt32()                       { return getRawValue<int32_t>(); }
    int getInt()                                { return getInt32(); }
    int32_t getInt32();
    int64_t getInt64();
    std::wstring getWString();
    void assertEof();

    // MSVC 2013 does not generate these automatically, so help it out.
    ReadBuffer(ReadBuffer &&other) :
        m_buf(std::move(other.m_buf)), m_off(other.m_off),
        m_exceptMode(other.m_exceptMode) {}
    ReadBuffer &operator=(ReadBuffer &&other) {
        m_buf = std::move(other.m_buf);
        m_off = other.m_off;
        m_exceptMode = other.m_exceptMode;
        return *this;
    }
};

#endif /* BUFFER_H */
