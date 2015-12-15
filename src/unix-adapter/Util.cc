// Copyright (c) 2015 Ryan Prichard
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

#include "Util.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "../shared/DebugClient.h"

// Write the entire buffer, restarting it as necessary.
bool writeAll(int fd, const void *buffer, size_t size) {
    size_t written = 0;
    while (written < size) {
        int ret = write(fd,
                        reinterpret_cast<const char*>(buffer) + written,
                        size - written);
        if (ret == -1 && errno == EINTR) {
            continue;
        }
        if (ret <= 0) {
            trace("write failed: "
                "fd=%d errno=%d size=%u written=%d ret=%d",
                fd,
                errno,
                static_cast<unsigned int>(size),
                static_cast<unsigned int>(written),
                ret);
            return false;
        }
        assert(static_cast<size_t>(ret) <= size - written);
        written += ret;
    }
    assert(written == size);
    return true;
}

bool writeStr(int fd, const char *str) {
    return writeAll(fd, str, strlen(str));
}
