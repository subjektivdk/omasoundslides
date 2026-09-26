TEMPLATE = app
TARGET = tst_core

QT = core testlib
CONFIG += c++17 console testcase warn_on
CONFIG -= app_bundle

include(../src/core/core.pri)

SOURCES += tst_core.cpp
