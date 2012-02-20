CFLAGS += -MMD
CXXFLAGS += -MMD

# Use gmake -n to see the command-lines gmake would run.

%.o : %.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

%.o : %.cc
	@echo Compiling $<
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<
