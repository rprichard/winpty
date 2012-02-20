UNAME_S = $(shell uname -s)

ifneq (,$(findstring MINGW,$(UNAME_S)))
    # MSYS/MINGW environment
    CC = i686-pc-msys-gcc
    CXX = i686-pc-msys-g++
else ifneq (,$(findstring CYGWIN,$(UNAME_S)))
    # Cygwin environment
    CC = gcc
    CXX = g++
else
    $(error Could not detect CYGWIN or MSYS/MINGW environment)
endif
