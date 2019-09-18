# Copyright (c) 2011-2015 Ryan Prichard
# Copyright (c) 2019 Lucio Andrés Illanes Albornoz <lucio@lucioillanes.de>
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

ALL_TARGETS += build/winpty-cygwin-agent.exe

$(eval $(call def_unix_target,cygwin-agent,-DWINPTY_AGENT_ASSERT))

CYGWIN_AGENT_OBJECTS = \
	build/cygwin-agent/agent/DebugShowInput.o \
	build/cygwin-agent/agent/InputMap.o \
	build/cygwin-agent/agent/main.o \
	build/cygwin-agent/cygwin-agent/Agent.o \
	build/cygwin-agent/cygwin-agent/AgentCreateDesktop.o \
	build/cygwin-agent/cygwin-agent/AgentCygwinPty.o \
	build/cygwin-agent/cygwin-agent/EventLoop.o \
	build/cygwin-agent/cygwin-agent/NamedPipe.o \
	build/cygwin-agent/shared/BackgroundDesktop.o \
	build/cygwin-agent/shared/Buffer.o \
	build/cygwin-agent/shared/DebugClient.o \
	build/cygwin-agent/shared/GenRandom.o \
	build/cygwin-agent/shared/OwnedHandle.o \
	build/cygwin-agent/shared/StringUtil.o \
	build/cygwin-agent/shared/WindowsSecurity.o \
	build/cygwin-agent/shared/WindowsVersion.o \
	build/cygwin-agent/shared/WinptyAssert.o \
	build/cygwin-agent/shared/WinptyException.o \
	build/cygwin-agent/shared/WinptyVersion.o

build/cygwin-agent/shared/WinptyVersion.o : build/gen/GenVersion.h

build/winpty-cygwin-agent.exe : $(CYGWIN_AGENT_OBJECTS) build/winpty.dll
	$(info Linking $@)
	@$(UNIX_CXX) $(UNIX_LDFLAGS) -o $@ $^

-include $(CYGWIN_AGENT_OBJECTS:.o=.d)
