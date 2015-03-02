#-------------------------------------------------
#
# Project created by QtCreator 2015-03-01T00:45:16
#
#-------------------------------------------------

QT       += core network

QT       -= gui

TARGET = ContinuationMonad
CONFIG   += console c++11
CONFIG   -= app_bundle

TEMPLATE = app

LIBS += -lhtmlcxx



SOURCES += main.cpp

HEADERS += \
    cont.h

OTHER_FILES += \
    README.md
