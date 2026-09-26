# The rendering engine, shared by the app and the tests.
INCLUDEPATH += $$PWD/..

HEADERS += \
    $$PWD/audiopreview.h \
    $$PWD/keybindings.h \
    $$PWD/ffmpegcommand.h \
    $$PWD/prepare.h \
    $$PWD/probe.h \
    $$PWD/project.h \
    $$PWD/projectmodel.h \
    $$PWD/renderer.h \
    $$PWD/timeline.h \
    $$PWD/transitions.h

SOURCES += \
    $$PWD/audiopreview.cpp \
    $$PWD/keybindings.cpp \
    $$PWD/ffmpegcommand.cpp \
    $$PWD/prepare.cpp \
    $$PWD/probe.cpp \
    $$PWD/project.cpp \
    $$PWD/projectmodel.cpp \
    $$PWD/renderer.cpp \
    $$PWD/timeline.cpp \
    $$PWD/transitions.cpp
