#-------------------------------------------------
#
# Project created by QtCreator 2014-09-05T21:15:35
#
#-------------------------------------------------

INCLUDEPATH += ./libs/opmapcontrol

include(./libs/opmapcontrol/opmapcontrol_external.pri)

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QT += network \
    opengl \
    svg \
    xml \
    webkit \
    concurrent \
    widgets \
    gui \
    serialport \
    sql \
    printsupport \
    webkitwidgets \
    quick

TARGET = OPMap
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp\
       QGCMapWidget.cc \
      QGCMapTool.cc \
       QGCMapToolBar.cc

HEADERS  += mainwindow.h\
    QGCMapWidget.h \
    MAV2DIcon.h \
    Waypoint2DIcon.h \
    QGCMapTool.h \
    QGCMapToolBar.h

FORMS    += mainwindow.ui \
    QGCMapTool.ui \
    QGCMapToolBar.ui
