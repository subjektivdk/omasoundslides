TEMPLATE = app
TARGET = omasoundslides
VERSION = 0.1.0

QT = core
CONFIG += c++17 console warn_on
CONFIG -= app_bundle

DEFINES += OMASOUNDSLIDES_VERSION=\\\"$$VERSION\\\"

include(src/core/core.pri)

SOURCES += src/main.cpp
