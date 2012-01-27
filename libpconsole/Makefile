include ../config.mk
include ../config-mingw.mk

LIBRARY = pconsole.dll
OBJECTS = pconsole.o PConsoleDebugClient.o
LDFLAGS += -shared -static-libgcc -static-libstdc++

CXXFLAGS += \
	-I../include \
	-DUNICODE \
	-D_UNICODE \
	-D_WIN32_WINNT=0x0501 \
	-DPCONSOLE \
	-fno-exceptions \
	-fno-rtti

$(LIBRARY) : $(OBJECTS)
	@echo Linking $@
	@$(CXX) $(LDFLAGS) -o $@ $^

.PHONY : clean
clean :
	rm -f $(LIBRARY) *.o *.d
