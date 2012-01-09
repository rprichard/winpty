#-------------------------------------------------
#
# Project created by QtCreator 2012-01-05T03:03:43
#
#-------------------------------------------------

QT       -= core gui

TARGET = PseudoConsole
TEMPLATE = lib
CONFIG -= exceptions rtti

QMAKE_LFLAGS = -static-libgcc

DEFINES += PSEUDOCONSOLE \
           _WIN32_WINNT=0x0501

SOURCES += PseudoConsole.cc \
           ../Shared/DebugClient.cc

HEADERS += PseudoConsole.h \
           ../Shared/DebugClient.h
