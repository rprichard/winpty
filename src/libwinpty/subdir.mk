# Copyright (c) 2011-2015 Ryan Prichard
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

ALL_TARGETS += build/winpty.dll

$(eval $(call def_mingw_target,libwinpty,-DCOMPILING_WINPTY_DLL))

LIBWINPTY_OBJECTS = \
	build/libwinpty/libwinpty/BackgroundDesktop.o \
	build/libwinpty/libwinpty/Util.o \
	build/libwinpty/libwinpty/WinptyException.o \
	build/libwinpty/libwinpty/winpty.o \
	build/libwinpty/shared/Buffer.o \
	build/libwinpty/shared/DebugClient.o \
	build/libwinpty/shared/GenRandom.o \
	build/libwinpty/shared/WindowsSecurity.o \
	build/libwinpty/shared/WinptyAssert.o \
	build/libwinpty/shared/winpty_snprintf.o

build/winpty.dll : $(LIBWINPTY_OBJECTS)
	@echo Linking $@
	@$(MINGW_CXX) $(MINGW_LDFLAGS) -shared -o $@ $^ -Wl,--out-implib,build/winpty.lib

-include $(LIBWINPTY_OBJECTS:.o=.d)
