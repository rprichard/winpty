include ../config.mk
include ../config-unix.mk

PROGRAM = pconsole
OBJECTS = main.o Shared.o
CXXFLAGS += -I../include
LDFLAGS += ../libpconsole/pconsole.dll

all : $(PROGRAM)

$(PROGRAM) : $(OBJECTS)
	@echo Linking $@
	@$(CXX) -o $@ $^ $(LDFLAGS)

.PHONY : clean
clean:
	rm -f $(PROGRAM) *.o *.d

-include $(OBJECTS:.o=.d)
