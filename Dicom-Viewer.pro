QT       += core gui widgets opengl

TARGET = DICOMViewer
TEMPLATE = app

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
DEFINES	+= QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

# --- VTK 8.2 ---

VTK_PATH = $$PWD/VTK8.2/Release

INCLUDEPATH += $$VTK_PATH/include/vtk-8.2
DEPENDPATH += $$VTK_PATH/include/vtk-8.2
QMAKE_LIBDIR += $$VTK_PATH/lib


LIBS += \
    -lvtkCommonCore-8.2 \
    -lvtkCommonDataModel-8.2 \
    -lvtkCommonExecutionModel-8.2 \
    -lvtkCommonMath-8.2 \
    -lvtkCommonTransforms-8.2 \
    -lvtkRenderingCore-8.2 \
    -lvtkRenderingOpenGL2-8.2 \
    -lvtkInteractionStyle-8.2 \
    -lvtkGUISupportQt-8.2 \
    -lvtkFiltersSources-8.2


# --- vtkDICOM ---
VTKDICOM_PATH = $$PWD/VTKDicom/Release
INCLUDEPATH += $$VTKDICOM_PATH/include
QMAKE_LIBDIR += $$VTKDICOM_PATH/lib

LIBS += -lvtkDICOM-8.2.0


# --- GDCM ---
GDCM_PATH = $$PWD/GDCM

INCLUDEPATH += $$GDCM_PATH/include/gdcm-2.6
QMAKE_LIBDIR += $$GDCM_PATH/lib

LIBS += \
    -lvtkgdcm \
    -lgdcmMSFF \
    -lgdcmIOD \
    -lgdcmDSED \
    -lgdcmDICT \
    -lgdcmCommon \
    -lgdcmCharls \
    -lgdcmjpeg8 \
    -lgdcmjpeg12 \
    -lgdcmjpeg16 \
    -lgdcmMEXD \
    -lgdcmexpat \
    -lgdcmgetopt \
    -lgdcmopenjpeg \
    -lgdcmzlib \
    -lsocketxx \



# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
