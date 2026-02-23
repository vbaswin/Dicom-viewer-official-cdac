TEMPLATE = app
TARGET = DICOMViewer


include(../shared_config.pri)

SOURCES += \
    drrviewer.cpp \
    main.cpp \
    mainwindow.cpp \
    mipviewer.cpp

HEADERS += \
    SphereInteractorStyle.h \
    drrviewer.h \
    mainwindow.h \
    mipviewer.h \
    precomp.h
