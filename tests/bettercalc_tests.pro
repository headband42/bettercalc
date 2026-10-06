QT += core gui testlib
CONFIG += c++17 testcase
TARGET = bettercalc_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/engine.h ../src/theme.h
SOURCES += bettercalc_tests.cpp ../src/backend.cpp ../src/engine.cpp ../src/theme.cpp
