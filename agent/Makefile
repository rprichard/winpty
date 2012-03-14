include ../config.mk
include ../config-mingw.mk
include ../config-qt.mk

PROGRAM = pconsole-agent
HEADERS =
OBJECTS = EventLoop.o NamedPipe.o Agent.o Terminal.o Win32Console.o AgentDebugClient.o main.o

OBJECTS += $(patsubst %.h,moc_%.o,$(HEADERS))
CXXFLAGS += -I$(QT_INCLUDE)/QtNetwork -D_WIN32_WINNT=0x0501
LDFLAGS += -lQtNetwork4 -static-libgcc -static-libstdc++

all : $(PROGRAM)

$(PROGRAM) : $(OBJECTS)
	@echo Linking $@
	@$(CXX) -o $@ $^ $(LDFLAGS)

.PHONY : clean
clean:
	rm -f $(PROGRAM) *.o *.d moc_*.cc

-include $(OBJECTS:.o=.d)
