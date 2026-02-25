#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDebug>
#include <QMainWindow>
#include <QObject>
#include "QVTKOpenGLNativeWidget.h"
#include "vtkColorTransferFunction.h"
#include "vtkDICOMDirectory.h"
#include "vtkDICOMReader.h"
#include "vtkFixedPointVolumeRayCastMapper.h"
#include "vtkGPUVolumeRayCastMapper.h"
#include "vtkGenericOpenGLRenderWindow.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkOpenGLGPUVolumeRayCastMapper.h"
#include "vtkPiecewiseFunction.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkSmartPointer.h"
#include "vtkSmartVolumeMapper.h"
#include "vtkSphereSource.h"
#include "vtkVolume.h"
#include "vtkVolumeProperty.h"
#include <iostream>

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    vtkImageData *getImageData();
    void setupVtk();
    void setupUI();

signals:
private:
    vtkNew<vtkDICOMDirectory> m_dir;
    vtkNew<vtkDICOMReader> m_reader;
    vtkNew<vtkPiecewiseFunction> m_opacityPiecewiseFunction;
    vtkNew<vtkColorTransferFunction> m_colorTransferFunction;
    vtkNew<vtkVolumeProperty> m_prop;
    // vtkNew<vtkSmartVolumeMapper> m_mapper;
    // vtkNew<vtkFixedPointVolumeRayCastMapper> m_mapper;
    // vtkNew<vtkGPUVolumeRayCastMapper> m_mapper;
    vtkNew<vtkOpenGLGPUVolumeRayCastMapper> m_mapper;
    vtkNew<vtkVolume> m_volume;
    vtkNew<vtkRenderer> m_renderer;
    vtkNew<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkNew<vtkRenderWindowInteractor> m_interactor;
    QVTKOpenGLNativeWidget *m_vtkWidget = nullptr;

};

#endif // MAINWINDOW_H
