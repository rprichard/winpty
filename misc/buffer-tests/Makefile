include ../../config-mingw.mk

CFLAGS += -MMD -Wall -Iharness -I../../shared
CXXFLAGS += -MMD -Wall -std=c++11 -Iharness -I../../shared
LDFLAGS += -static -static-libgcc -static-libstdc++

# Use gmake -n to see the command-lines gmake would run.

build/%.o : %.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

build/%.o : %.cc
	@echo Compiling $<
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

build/%.o : ../../shared/%.cc
	@echo Compiling $<
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

build/%.o : harness/%.cc
	@echo Compiling $<
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

.PRECIOUS : build/%.o

COMMON_OBJECTS = \
	build/DebugClient.o \
	build/Event.o \
	build/HookAssert.o \
	build/ShmemParcel.o \
	build/Spawn.o \
	build/UnicodeConversions.o \
	build/Util.o

WORKER_OBJECTS = build/Worker.o
TEST_OBJECTS = build/TestCommon.o

all : \
	build/Win7Bug_InheritHandles.exe \
	build/Win7Bug_RaceCondition.exe \
	build/TestHandleInheritance.exe \
	build/Worker.exe

Worker.exe : $(WORKER_OBJECTS) $(COMMON_OBJECTS)
	@echo Linking $@
	@$(CXX) -o $@ $^ $(LDFLAGS)

%.exe : %.o $(TEST_OBJECTS) $(COMMON_OBJECTS)
	@echo Linking $@
	@$(CXX) -o $@ $^ $(LDFLAGS)

.PHONY : clean
clean:
	rm -f build/*.exe build/*.o build/*.d

-include build/*.d
