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

# Use make -n to see the actual command-lines make would run.

default : all

PREFIX ?= /usr/local
UNIX_ADAPTER_EXE ?= console.exe

# Include config.mk but complain if it hasn't been created yet.
ifeq "$(wildcard config.mk)" ""
    $(error config.mk does not exist.  Please run ./configure)
endif

MINGW_ENABLE_CXX11_FLAG ?= -std=c++11

include config.mk

COMMON_CXXFLAGS := $(COMMON_CXXFLAGS) \
	-DWINPTY_VERSION=$(shell cat VERSION.txt | tr -d '\r\n') \
	-DWINPTY_VERSION_SUFFIX=$(VERSION_SUFFIX) \
	-DWINPTY_COMMIT_HASH=$(COMMIT_HASH) \
	-MMD -Wall \
	-DUNICODE \
	-D_UNICODE \
	-DWINVER=0x0501 \
	-D_WIN32_WINNT=0x0501

UNIX_CXXFLAGS := $(UNIX_CXXFLAGS) \
	$(COMMON_CXXFLAGS)

MINGW_CXXFLAGS := $(MINGW_CXXFLAGS) \
	$(COMMON_CXXFLAGS) \
	-O2 \
	$(MINGW_ENABLE_CXX11_FLAG)

MINGW_LDFLAGS += -static -static-libgcc -static-libstdc++
UNIX_LDFLAGS += $(UNIX_LDFLAGS_STATIC)

define def_unix_target
build/$1/%.o : src/%.cc VERSION.txt
	@echo Compiling $$<
	@mkdir -p $$(dir $$@)
	@$$(UNIX_CXX) $$(UNIX_CXXFLAGS) $2 -I src/include -c -o $$@ $$<
endef

define def_mingw_target
build/$1/%.o : src/%.cc VERSION.txt
	@echo Compiling $$<
	@mkdir -p $$(dir $$@)
	@$$(MINGW_CXX) $$(MINGW_CXXFLAGS) $2 -I src/include -c -o $$@ $$<
endef

include src/subdir.mk

all : $(ALL_TARGETS)

tests : $(TEST_PROGRAMS)

install : all
	mkdir -p $(PREFIX)/bin
	install -m 755 -p -s build/$(UNIX_ADAPTER_EXE) $(PREFIX)/bin
	install -m 755 -p -s build/winpty.dll $(PREFIX)/bin
	install -m 755 -p -s build/winpty-agent.exe $(PREFIX)/bin
	install -m 755 -p -s build/winpty-debugserver.exe $(PREFIX)/bin

clean :
	rm -fr build

clean-msvs :
	rm -fr src/Default
	rm -f src/*.vcxproj
	rm -f src/*.vcxproj.filters
	rm -f src/*.sln

distclean : clean
	rm -f config.mk

.PHONY : default all tests install clean clean-msvs distclean

src/%.h :
	@echo "Missing header file $@ (stale dependency file?)"
