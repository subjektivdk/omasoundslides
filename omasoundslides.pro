TEMPLATE = app
TARGET = omasoundslides
VERSION = 0.1.0

QT += core gui qml quick quickcontrols2 multimedia dbus concurrent
CONFIG += c++17 warn_on
CONFIG -= app_bundle

DEFINES += OMASOUNDSLIDES_VERSION=\\\"$$VERSION\\\"

include(src/core/core.pri)

HEADERS += \
    src/app/controller.h \
    src/app/portalfilepicker.h \
    src/app/theme.h \
    src/app/waveformitem.h \
    src/cli.h

SOURCES += \
    src/app/controller.cpp \
    src/app/portalfilepicker.cpp \
    src/app/theme.cpp \
    src/app/waveformitem.cpp \
    src/cli.cpp \
    src/main.cpp

RESOURCES += src/qml/qml.qrc
