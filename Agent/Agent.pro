#-------------------------------------------------
#
# Project created by QtCreator 2011-10-30T22:44:39
#
#-------------------------------------------------

QT       += core

QT       -= gui

QT       += network

TARGET = Agent
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cc \
    Agent.cc \
    ../Shared/DebugClient.cc \
    Win32Console.cc

HEADERS += \
    Agent.h \
    ../Shared/DebugClient.h \
    Win32Console.h


