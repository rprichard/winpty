UNAME_S = $(shell uname -s)

ifneq (,$(findstring MINGW,$(UNAME_S)))
    # MSYS/MINGW environment
    CC = mingw32-gcc
    CXX = mingw32-g++
else ifneq (,$(findstring CYGWIN,$(UNAME_S)))
    # Cygwin environment
    CC = i686-pc-mingw32-gcc
    CXX = i686-pc-mingw32-g++
else
    $(error Could not detect CYGWIN or MSYS/MINGW environment)
endif
