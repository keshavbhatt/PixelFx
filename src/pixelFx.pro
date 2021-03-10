#-------------------------------------------------
#
# Project created by QtCreator 2020-11-15T19:04:54
#
#-------------------------------------------------

QT       += core gui xml network concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets


# Set program version
VERSION = 0.0.1
DEFINES += VERSIONSTR=\\\"$${VERSION}\\\"
DEFINES += APPNAMESTR=\\\"$${TARGET}\\\"

CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT

include(lib/color_widgets.pri)
include(lib/ImageCropper/ImageCropper.pri)
include(lib/EasyExif/EasyExif.pri)

TARGET = pixelfx
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

SOURCES += \
        apppreferences.cpp \
        corefilters.cpp \
        coreslider.cpp \
        cropwidget.cpp \
        gift.cpp \
        graphicsview.cpp \
        graphicsviewcontrols.cpp \
        imageinfowidget.cpp \
        imageloader.cpp \
        main.cpp \
        mainwindow.cpp \
        rungaurd.cpp \
        savedialog.cpp \
        widgets/elidedLabel/elidedlabel.cpp \
        widgets/scope/Histogram.cpp \
        widgets/scope/ScopesLabel.cpp \
        widgets/scope/VectorScope.cpp \
        widgets/scope/WaveFormMonitor.cpp \
        widgets/waitingSpinner/waitingspinnerwidget.cpp

HEADERS += \
        apppreferences.h \
        corefilters.h \
        coreslider.h \
        cropwidget.h \
        gift.h \
        graphicsview.h \
        graphicsviewcontrols.h \
        imageinfowidget.h \
        imageloader.h \
        mainwindow.h \
        rungaurd.h \
        savedialog.h \
        widgets/elidedLabel/elidedlabel.h \
        widgets/scope/Histogram.h \
        widgets/scope/ScopesLabel.h \
        widgets/scope/VectorScope.h \
        widgets/scope/WaveFormMonitor.h \
        widgets/waitingSpinner/waitingspinnerwidget.h

FORMS += \
        apppreferences.ui \
        corefilters.ui \
        cropwidget.ui \
        graphicsviewcontrols.ui \
        imageinfowidget.ui \
        mainwindow.ui \
        savedialog.ui

# Default rules for deployment.
isEmpty(PREFIX){
 PREFIX = /usr
}

BINDIR  = $$PREFIX/bin
DATADIR = $$PREFIX/share

target.path = $$BINDIR

icon.files = icons/$$TARGET.png
icon.path = $$DATADIR/icons/hicolor/512x512/apps/

desktop.files = $$TARGET.desktop
desktop.path = $$DATADIR/applications/

INSTALLS += target icon desktop

RESOURCES += \
    icons.qrc \
    theme/qbreeze.qrc
