TEMPLATE = app

CONFIG += warn_on qt resources silent c++17

QT += core network xml gui widgets dbus

SOURCES += src/main.cpp \
    src/forms/frmprogress.cpp \
    src/forms/frmlistsubtitles.cpp \
    src/forms/frmsummary.cpp \
    src/forms/frmscan.cpp \
    src/forms/frmoptions.cpp \
    src/forms/frmabout.cpp \
    src/forms/frmnapiprojektconfig.cpp \
    src/forms/frmopensubtitlesconfig.cpp \
    src/forms/frmnapisy24config.cpp \
    src/forms/frmconvert.cpp \
    src/forms/frmnapiprojektupload.cpp \
    src/forms/frmnapiprojektscan.cpp \
    src/forms/subdatawidget.cpp \
    src/qcumber/qmanagedrequest.cpp \
    src/qcumber/qmanagedsocket.cpp \
    src/qcumber/qsingleapplication.cpp \
    src/qnapiopendialog.cpp \
    src/qnapiapp.cpp \
    src/guimain.cpp \
    src/utils/notificationservice.cpp

HEADERS += src/forms/frmprogress.h \
    src/forms/frmlistsubtitles.h \
    src/forms/frmsummary.h \
    src/forms/frmscan.h \
    src/forms/frmoptions.h \
    src/forms/frmabout.h \
    src/forms/frmnapiprojektconfig.h \
    src/forms/frmopensubtitlesconfig.h \
    src/forms/frmnapisy24config.h \
    src/forms/frmconvert.h \
    src/forms/frmnapiprojektupload.h \
    src/forms/frmnapiprojektscan.h \
    src/forms/subdatawidget.h \
    src/qcumber/qmanagedrequest.h \
    src/qcumber/qmanagedsocket.h \
    src/qcumber/qsingleapplication.h \
    src/qcumber/qcumber.h \
    src/qcheckedlistwidget.h \
    src/qnapithread.h \
    src/qnapiopendialog.h \
    src/qnapiapp.h \
    src/guimain.h \
    src/utils/notificationservice.h

FORMS += ui/frmprogress.ui \
    ui/frmlistsubtitles.ui \
    ui/frmsummary.ui \
    ui/frmscan.ui \
    ui/frmoptions.ui \
    ui/frmabout.ui \
    ui/frmconvert.ui \
    ui/frmnapiprojektupload.ui \
    ui/frmnapiprojektscan.ui \
    ui/subDataWidget.ui \
    ui/napiprojekt/frmnapiprojektconfig.ui \
    ui/opensubtitles/frmopensubtitlesconfig.ui \
    ui/napisy24/frmnapisy24config.ui

RESOURCES += res/resources.qrc
UI_DIR = tmp
MOC_DIR = tmp
RCC_DIR = tmp
OBJECTS_DIR = tmp
INCLUDEPATH = src

include(../qnapi.pri)
include(../libqnapi/libqnapi.pri)

TARGET = subnapi
DESTDIR = ../
CONFIG += link_pkgconfig
PKGCONFIG += libmediainfo

SOURCES += src/qcumber/qinterprocesschannel.cpp
HEADERS += src/qcumber/qinterprocesschannel.h

target.path = $${INSTALL_PREFIX}/bin
INSTALLS += target
