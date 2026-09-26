TEMPLATE = app
TARGET = tst_gui

QT = core gui qml quick quickcontrols2 multimedia dbus concurrent testlib
CONFIG += c++17 console testcase warn_on
CONFIG -= app_bundle

include(../../src/core/core.pri)

INCLUDEPATH += ../../src

HEADERS += \
    ../../src/app/controller.h \
    ../../src/app/portalfilepicker.h \
    ../../src/app/theme.h \
    ../../src/app/waveformitem.h

SOURCES += \
    ../../src/app/controller.cpp \
    ../../src/app/portalfilepicker.cpp \
    ../../src/app/theme.cpp \
    ../../src/app/waveformitem.cpp \
    tst_gui.cpp

RESOURCES += ../../src/qml/qml.qrc
