include ../config.mk
include ../config-mingw.mk
include ../config-qt.mk

PROGRAM = testnet-server
HEADERS = Server.h Session.h AgentClient.h
OBJECTS = Server.o Session.o AgentClient.o TestNetServerDebugClient.o main.o

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
