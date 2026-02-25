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
    m_mapper->SetInputConnection(m_reader->GetOutputPort());
    // m_mapper->AutoAdjustSampleDistancesOff();
    // m_mapper->SetSampleDistance(0.5);
    m_mapper->SetBlendModeToComposite();
    // m_mapper->SetBlendModeToMaximumIntensity();

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

    m_prop->SetScalarOpacity(m_opacityPiecewiseFunction);
    m_prop->SetColor(m_colorTransferFunction);
    m_prop->ShadeOn();
    m_prop->SetInterpolationTypeToLinear();

    m_volume->SetMapper(m_mapper);
    m_volume->SetProperty(m_prop);

    // m_camera->SetViewUp(0, 0, -1);
    // m_camera->SetPosition(0, -1, 0);
    // m_camera->SetFocalPoint(0, 0, 0);

    m_renderer->AddVolume(m_volume);
    m_renderer->SetActiveCamera(m_camera);
    m_renderer->SetBackground(23.0 / 255.0, 146.0 / 255.0, 153.0 / 255.0);

    // m_camera->Azimuth(90.0);
    // m_camera->Elevation(30.0);
    // m_camera->Dolly(1.5);
    // m_renderer->ResetCameraClippingRange();

    m_renderWindow->AddRenderer(m_renderer);
    m_renderWindow->SetWindowName("openglraycastmapper");

    m_vtkWidget->SetRenderWindow(m_renderWindow);

    m_renderer->ResetCamera();
    // m_renderWindow->GetInteractor()->Initialize();
    // qDebug() << "GPU mode: " << (m_mapper->GetLastUsedRenderMode() == 0);
}

void MainWindow::setupUI()
{
    this->setCentralWidget(m_vtkWidget);
}
