QT += core gui qml quick quickcontrols2 dbus

CONFIG += c++17 release
TARGET = bettercalc
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/engine.h \
    src/systemtheme.h \
    src/theme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/engine.cpp \
    src/systemtheme.cpp \
    src/theme.cpp

RESOURCES += src/resources.qrc
