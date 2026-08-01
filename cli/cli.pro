TEMPLATE = app

CONFIG += warn_on qt resources silent c++17 console

QT += core network xml
QT -= gui

SOURCES += src/main.cpp \
    src/clisubtitlesdownloader.cpp \
    src/clisubtitlesuploader.cpp \
    src/climain.cpp

HEADERS += \
    src/clisubtitlesdownloader.h \
    src/clisubtitlesuploader.h \
    src/climain.h

RESOURCES += res/resources.qrc

MOC_DIR = tmp
RCC_DIR = tmp
OBJECTS_DIR = tmp
INCLUDEPATH = src

include(../qnapi.pri)
include(../libqnapi/libqnapi.pri)

TARGET = subnapic
DESTDIR = $${OUTDIR}
CONFIG += link_pkgconfig
PKGCONFIG += libmediainfo

target.path = $${INSTALL_PREFIX}/bin
INSTALLS += target
