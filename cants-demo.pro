# See the file "LICENSE.txt" for the full license governing this code.

QT += core gui serialport widgets

TARGET = cants-demo
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_USE_QSTRINGBUILDER

# Disable info and debug output in release build.
CONFIG(release, debug|release) {
    DEFINES += QT_NO_DEBUG_OUTPUT
    DEFINES += QT_NO_INFO_OUTPUT
}

CONFIG += c++14 strict_c++ warn_on #console

INCLUDEPATH += \
        app/ \
        include

SOURCES += \
        app/main.cpp \
        app/mainwindow.cpp \
        src/commdriver.cpp \
        src/canframe.cpp \
        src/cantsutils.cpp \
        src/skyslip.cpp \
        src/can_ts.cpp \
        src/can_ts_tc.cpp \
        src/can_ts_tm.cpp \
        src/can_ts_sb.cpp \
        src/can_ts_gb.cpp \
        src/can_ts_ts.cpp \
        src/can_ts_un.cpp \
        src/cantsframe.cpp

HEADERS += \
        app/mainwindow.h \
        include/canframe.h \
        include/cantsutils.h \
        include/commdriver.h \
        include/skyslip.h \
        include/can_ts.h \
        include/cantsframe.h

FORMS += \
        gui/mainwindow.ui

DISTFILES += \
        res/rocket.ico

RC_ICONS += \
        res/rocket.ico
