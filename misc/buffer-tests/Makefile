include ../../config-mingw.mk

# Pass -Wno-format to disable format checking because gcc complains about
# %I64x.  I can't use %lld because it's broken on MinGW-on-WinXP, though it
# works on later OSs.  %I64x works everywhere with MinGW, at runtime.  I
# need to find a better way to do this int-to-string conversion.
CFLAGS +=   -MMD -Wall  -Iharness -I../../shared
CXXFLAGS += -MMD -Wall  -Iharness -I../../shared -std=c++11 -Wno-format
LDFLAGS += -static -static-libgcc -static-libstdc++

# To disable PCH, just comment out these two lines.
PCHFLAGS = -include build/pch.h
PCHDEPS = build/pch.h.gch

# Use gmake -n to see the command-lines gmake would run.

COMMON_OBJECTS = \
	build/DebugClient.o \
	build/Event.o \
	build/NtHandleQuery.o \
	build/ShmemParcel.o \
	build/Spawn.o \
	build/UnicodeConversions.o \
	build/Util.o \
	build/WinptyAssert.o \
	build/winpty_wcsnlen.o

WORKER_OBJECTS = build/WorkerProgram.o
TEST_OBJECTS = \
	build/RemoteHandle.o \
	build/RemoteWorker.o \
	build/TestUtil.o

include tests.mk

.PHONY : all
all : $(TESTS) build/Worker.exe

.PHONY : clean
clean:
	rm -f build/*.exe build/*.o build/*.d build/*.gch

build/pch.h.gch : harness/pch.h
	@echo Compiling PCH $<
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

build/%.o : %.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

build/%.o : %.cc $(PCHDEPS)
	@echo Compiling $<
	@$(CXX) $(PCHFLAGS) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

build/%.o : ../../shared/%.cc $(PCHDEPS)
	@echo Compiling $<
	@$(CXX) $(PCHFLAGS) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

build/%.o : harness/%.cc $(PCHDEPS)
	@echo Compiling $<
	@$(CXX) $(PCHFLAGS) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

.PRECIOUS : build/%.o

build/Worker.exe : $(WORKER_OBJECTS) $(COMMON_OBJECTS)
	@echo Linking $@
	@$(CXX) -o $@ $^ $(LDFLAGS)

build/%.exe : build/%.o $(TEST_OBJECTS) $(COMMON_OBJECTS)
	@echo Linking $@
	@$(CXX) -o $@ $^ $(LDFLAGS)

-include build/*.d
