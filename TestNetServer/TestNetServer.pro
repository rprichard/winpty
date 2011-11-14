#-------------------------------------------------
#
# Project created by QtCreator 2011-11-03T01:45:58
#
#-------------------------------------------------

QT       += core
QT       -= gui
QT       += network

TARGET = TestNetServer
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cc \
    ../Shared/AgentClient.cc \
    ../Shared/DebugClient.cc \
    Session.cc \
    Server.cc

HEADERS += \
    ../Shared/AgentClient.h \
    ../Shared/AgentMsg.h \
    ../Shared/DebugClient.h \
    Session.h \
    Server.h
