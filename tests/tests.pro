TEMPLATE = app
TARGET = tst_core

QT = core gui testlib dbus concurrent
CONFIG += c++17 console testcase warn_on
CONFIG -= app_bundle

include(../src/core/core.pri)

INCLUDEPATH += ../src

HEADERS += \
    ../src/app/controller.h \
    ../src/app/portalfilepicker.h

SOURCES += \
    ../src/app/controller.cpp \
    ../src/app/portalfilepicker.cpp \
    tst_core.cpp
