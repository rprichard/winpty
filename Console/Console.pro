#-------------------------------------------------
#
# Project created by QtCreator 2011-10-30T22:46:25
#
#-------------------------------------------------

QT       += core gui network

TARGET = Console
TEMPLATE = app


SOURCES += main.cc \
    ConsoleWindow.cc \
    ../Shared/AgentClient.cc \
    ../Shared/DebugClient.cc \
    TextWidget.cc

HEADERS  += ConsoleWindow.h \
    ../Shared/AgentClient.h \
    ../Shared/AgentMsg.h \
    TextWidget.h

FORMS    += ConsoleWindow.ui
