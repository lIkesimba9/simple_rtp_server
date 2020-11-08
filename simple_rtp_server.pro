TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.cpp

QMAKE_LFLAGS += -pthread
PKGCONFIG += gstreamer-1.0 \
            glib-2.0 \
            gio-2.0

INCLUDEPATH += /usr/include/gstreamer-1.0/
CONFIG += link_pkgconfig
PKGCONFIG += gstreamer-1.0 glib-2.0 gobject-2.0 gstreamer-app-1.0 gstreamer-pbutils-1.0
CCFLAG += gstrtp-1.0
unix: CONFIG += link_pkgconfig
unix: PKGCONFIG += /usr/lib/x86_64-linux-gnu/pkgconfig/gstreamer-1.0.pc
unix: PKGCONFIG += /usr/lib/x86_64-linux-gnu/pkgconfig/gstreamer-plugins-base-1.0.pc
unix: PKGCONFIG += /usr/lib/x86_64-linux-gnu/pkgconfig/gstreamer-plugins-good-1.0.pc
unix: PKGCONFIG += /usr/lib/x86_64-linux-gnu/pkgconfig/gstreamer-rtp-1.0.pc

