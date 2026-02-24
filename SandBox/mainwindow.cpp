#include "mainwindow.h"
#include "vtkCamera.h"
#include "vtkImageViewer2.h"
#include "vtkSampleFunction.h"
#include "vtkSphere.h"
#include "vtkSphereSource.h"

const char *dirPath = "C:/Users/cdac/Projects/SE2dcm";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow{parent}
{
    m_vtkWidget = new QVTKOpenGLNativeWidget(this);
    qDebug() << "Constructor";
    this->resize(1980, 1080);
    setupUI();
    setupVtk();
}

vtkImageData *MainWindow::getImageData()
{
    m_dir->SetDirectoryName(dirPath);
    m_dir->Update();
    if (m_dir->GetNumberOfSeries() == 0) {
        // qDebug() << "Hello Before";
        std::cerr << "No dicom series found in the directory ";
        return nullptr;
    }
    // qDebug() << "Hello After";
    m_reader->SetFileNames(m_dir->GetFileNamesForSeries(0));
    m_reader->Update();

    double range[2];
    m_reader->GetOutput()->GetScalarRange(range);
    qDebug() << "Dicom scalar range: " << range[0] << " to " << range[1];

    return m_reader->GetOutput();
}

void MainWindow::setupVtk()
{
    // getImageData();
    // m_mapper->SetInputData(getImageData());

    m_dir->SetDirectoryName(dirPath);
    m_dir->Update();
    if (m_dir->GetNumberOfSeries() == 0) {
        // qDebug() << "Hello Before";
        std::cerr << "No dicom series found in the directory ";
    }
    // qDebug() << "Hello After";
    m_reader->SetFileNames(m_dir->GetFileNamesForSeries(0));
    m_reader->Update();

    double range[2];
    m_reader->GetOutput()->GetScalarRange(range);
    // qDebug() << "Dicom scalar range: " << range[0] << " to " << range[1];

    // int *dims = m_reader->GetOutput()->GetDimensions();
    // qDebug() << "Dimensions " << dims[0] << dims[1] << dims[2];

    m_opacityPiecewiseFunction->RemoveAllPoints();
    m_opacityPiecewiseFunction->AddPoint(range[0], 0.0); // water
    m_opacityPiecewiseFunction->AddPoint(-1000, 0.0);    // water
    m_opacityPiecewiseFunction->AddPoint(0, 0.05);       // water
    m_opacityPiecewiseFunction->AddPoint(300, 0.2);      // bone
    m_opacityPiecewiseFunction->AddPoint(700, 0.7);      // bone
    m_opacityPiecewiseFunction->AddPoint(1500, 1.00);    // dense bone

    m_colorTransferFunction->RemoveAllPoints();
    m_colorTransferFunction->AddRGBPoint(range[0], 0.0, 0.0, 0.0);
    m_colorTransferFunction->AddRGBPoint(range[1], 1.0, 1.0, 1.0);

    vtkNew<vtkPiecewiseFunction> opacityVol;
    vtkNew<vtkColorTransferFunction> colorVol;

    // -ve inside sphere opaque
    opacityVol->AddPoint(-1000.0, 1.0);
    opacityVol->AddPoint(0.0, 1.0);
    opacityVol->AddPoint(0.01, 0.0);
    opacityVol->AddPoint(1000.0, 0.0);

    colorVol->AddRGBPoint(-100.0, 1.0, 0.0, 0.0);
    colorVol->AddRGBPoint(0.0, 1.0, 0.0, 0.0);
    colorVol->AddRGBPoint(100.0, 0.0, 1.0, 0.0);

    vtkNew<vtkVolumeProperty> volProps;
    volProps->SetScalarOpacity(opacityVol);
    volProps->SetColor(colorVol);

    vtkNew<vtkSphere> sphere;
    sphere->SetRadius(1.0);
    sphere->SetCenter(0.0, 0.0, 0.0);

    vtkNew<vtkSampleFunction> sampleFn;
    sampleFn->SetImplicitFunction(sphere);
    sampleFn->SetModelBounds(-2, 2, -2, 2, -2, 2);
    sampleFn->SetSampleDimensions(80, 80, 80);
    sampleFn->ComputeNormalsOff();
    sampleFn->Update();

    m_mapperVol->SetInputConnection(sampleFn->GetOutputPort());
    m_vol->SetMapper(m_mapperVol);
    m_vol->SetProperty(volProps);

    m_prop->SetScalarOpacity(m_opacityPiecewiseFunction);
    m_prop->SetColor(m_colorTransferFunction);
    m_prop->ShadeOn();
    m_prop->SetInterpolationTypeToLinear();

    m_mapper->SetInputConnection(m_reader->GetOutputPort());
    m_mapper->SetAutoAdjustSampleDistances(1);

    m_volume->SetMapper(m_mapper);
    m_volume->SetProperty(m_prop);

    vtkImageData *samp = sampleFn->GetOutput();
    double srange[2];
    samp->GetScalarRange(srange);
    int dims[3];
    samp->GetDimensions(dims);
    double origin[3];
    samp->GetOrigin(origin);

    // m_renderer->AddVolume(m_volume);
    qDebug() << "Range: " << srange[0] << " to " << srange[1];
    m_renderer->AddVolume(m_vol);

    m_renderWindow->AddRenderer(m_renderer);

    m_vtkWidget->SetRenderWindow(m_renderWindow);

    m_renderer->ResetCamera();
    // qDebug() << "GPU mode: " << (m_mapper->GetLastUsedRenderMode() == 0);
}

void MainWindow::setupUI()
{
    this->setCentralWidget(m_vtkWidget);
}
