include ../config.mk
include ../config-mingw.mk

PROGRAM = pconsole-agent

OBJECTS = \
	EventLoop.o \
	NamedPipe.o \
	Agent.o \
	AgentAssert.o \
	Terminal.o \
	Win32Console.o \
	ConsoleInput.o \
	AgentDebugClient.o \
	Coord.o \
	SmallRect.o \
	main.o

CXXFLAGS += \
	-DUNICODE \
	-D_UNICODE \
	-D_WIN32_WINNT=0x0501 \
	-fno-exceptions \
	-fno-rtti

LDFLAGS += -static-libgcc -static-libstdc++

all : $(PROGRAM)

$(PROGRAM) : $(OBJECTS)
	@echo Linking $@
	@$(CXX) -o $@ $^ $(LDFLAGS)

.PHONY : clean
clean:
	rm -f $(PROGRAM) *.o *.d moc_*.cc

-include $(OBJECTS:.o=.d)
