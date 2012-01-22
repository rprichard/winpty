include ../config.mk
include ../config-mingw.mk
include ../config-qt.mk

PROGRAM = pconsole-agent
HEADERS = Agent.h Terminal.h Win32Console.h
OBJECTS = Agent.o Terminal.o Win32Console.o AgentDebugClient.o main.o

OBJECTS += $(patsubst %.h,moc_%.o,$(HEADERS))
CXXFLAGS += -I$(QT_INCLUDE)/QtNetwork
LDFLAGS += -lQtNetwork4

all : $(PROGRAM)

$(PROGRAM) : $(OBJECTS)
	@echo Linking $@
	@$(CXX) -o $@ $^ $(LDFLAGS)

.PHONY : clean
clean:
	rm -f $(PROGRAM) *.o *.d moc_*.cc

-include $(OBJECTS:.o=.d)
