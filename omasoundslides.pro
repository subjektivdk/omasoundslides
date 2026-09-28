# SPDX-FileCopyrightText: 2026 Martin Jensen
#
# SPDX-License-Identifier: MIT

TEMPLATE = app
TARGET = omasoundslides
VERSION = 0.1.0

QT += core gui qml quick quickcontrols2 multimedia dbus concurrent
CONFIG += c++17 warn_on
CONFIG -= app_bundle

# The version shown by --version: from git (bin/version), else VERSION
# above. Written to version.h only when it changes, so make recompiles
# exactly the files that show it.
APP_VERSION = $$system($$PWD/bin/version --dirty)
isEmpty(APP_VERSION): APP_VERSION = $$VERSION
VERSION_HEADER = "$${LITERAL_HASH}define OMASOUNDSLIDES_VERSION \"$$APP_VERSION\""
!equals(VERSION_HEADER, $$cat($$OUT_PWD/version.h, blob)) {
    write_file($$OUT_PWD/version.h, VERSION_HEADER)|error("Cannot write version.h")
}
INCLUDEPATH += $$OUT_PWD

include(src/core/core.pri)

HEADERS += \
    src/app/controller.h \
    src/app/portalfilepicker.h \
    src/app/theme.h \
    src/app/waveformitem.h \
    src/cli.h \
    src/cliedit.h

SOURCES += \
    src/app/controller.cpp \
    src/app/portalfilepicker.cpp \
    src/app/theme.cpp \
    src/app/waveformitem.cpp \
    src/cli.cpp \
    src/cliedit.cpp \
    src/main.cpp

RESOURCES += src/qml/qml.qrc
