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
    SphereInteractorStyle.h \
    mainwindow.h



# --- VTK 8.2 (MSVC 2019-compatible x64 build) ---
 VTK_INSTALL = C:/Users/aswin/Projects/VTK8.2/install

 INCLUDEPATH += $$VTK_INSTALL/include/vtk-8.2
 DEPENDPATH  += $$VTK_INSTALL/include/vtk-8.2
 QMAKE_LIBDIR += $$VTK_INSTALL/lib

 LIBS += \
     -lvtkCommonCore-8.2 \
     -lvtkCommonDataModel-8.2 \
     -lvtkCommonExecutionModel-8.2 \
     -lvtkCommonMath-8.2 \
     -lvtkCommonTransforms-8.2 \
     -lvtkCommonMisc-8.2 \
     -lvtkRenderingCore-8.2 \
     -lvtkRenderingImage-8.2 \
     -lvtkRenderingAnnotation-8.2 \
     -lvtkRenderingOpenGL2-8.2 \
     -lvtkRenderingFreeType-8.2 \
     -lvtkInteractionStyle-8.2 \
     -lvtkInteractionImage-8.2 \
     -lvtkGUISupportQt-8.2 \
     -lvtkFiltersSources-8.2 \
     -lvtkIOImage-8.2 \
     -lvtkIOCore-8.2 \
     -lvtkImagingCore-8.2 \
     -lvtkImagingColor-8.2 \
     -lvtkInteractionWidgets-8.2 \
     -lvtkFiltersCore-8.2

 # --- vtkDICOM 0.8.13 (MSVC 2019-compatible x64 build) ---
 VTKDICOM_INSTALL = C:/Users/aswin/Projects/VTKDicom/install

 INCLUDEPATH += $$VTKDICOM_INSTALL/include
 QMAKE_LIBDIR += $$VTKDICOM_INSTALL/lib

 LIBS += -lvtkDICOM-8.2.0

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target




# # --- GDCM ---
# GDCM_PATH = $$PWD/../GDCM

# INCLUDEPATH += $$GDCM_PATH/include/gdcm-2.6
# QMAKE_LIBDIR += $$GDCM_PATH/lib

# LIBS += \
#     -lvtkgdcm \
#     -lgdcmMSFF \
#     -lgdcmIOD \
#     -lgdcmDSED \
#     -lgdcmDICT \
#     -lgdcmCommon \
#     -lgdcmCharls \
#     -lgdcmjpeg8 \
#     -lgdcmjpeg12 \
#     -lgdcmjpeg16 \
#     -lgdcmMEXD \
#     -lgdcmexpat \
#     -lgdcmgetopt \
#     -lgdcmopenjpeg \
#     -lgdcmzlib \
#     -lsocketxx \



# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
