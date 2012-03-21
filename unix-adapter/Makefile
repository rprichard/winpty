include ../config.mk
include ../config-unix.mk

PROGRAM = ../build/pconsole.exe
OBJECTS = main.o Shared.o
CXXFLAGS += -I../include
LDFLAGS += ../build/pconsole.dll

all : $(PROGRAM)

$(PROGRAM) : $(OBJECTS)
	@echo Linking $@
	@$(CXX) -o $@ $^ $(LDFLAGS)

.PHONY : clean
clean:
	rm -f $(PROGRAM) *.o *.d

-include $(OBJECTS:.o=.d)
