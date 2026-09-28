# SPDX-FileCopyrightText: 2026 Martin Jensen
#
# SPDX-License-Identifier: MIT

TEMPLATE = app
TARGET = tst_core

QT = core gui testlib dbus concurrent
CONFIG += c++17 console testcase warn_on
CONFIG -= app_bundle

include(../src/core/core.pri)

INCLUDEPATH += ../src

HEADERS += \
    ../src/app/controller.h \
    ../src/app/portalfilepicker.h \
    ../src/app/theme.h \
    ../src/cliedit.h

SOURCES += \
    ../src/app/controller.cpp \
    ../src/app/portalfilepicker.cpp \
    ../src/app/theme.cpp \
    ../src/cliedit.cpp \
    tst_core.cpp
