#include "mainwindow.h"

#include "vtkConeSource.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
// #include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    // , ui(new Ui::MainWindow)
{
    // ui->setupUi(this);
    setWindowTitle("DICOM Viewer");
    resize(1024, 768);

    setupVTKWidget();
     setupTestCone();
}

MainWindow::~MainWindow()
{
    // delete ui;
}

void MainWindow::setupVTKWidget() {
    m_vtkWidget = new QVTKOpenGLNativeWidget(this);
    setCentralWidget(m_vtkWidget);


    m_vtkWidget->SetRenderWindow(m_renderWindow);

    m_renderer->SetBackground(0.1, 0.1,0.1);
    m_renderWindow->AddRenderer(m_renderer);
}

void MainWindow::setupTestCone() {
    vtkNew<vtkConeSource> coneSource;
    coneSource->SetHeight(3.0);
    coneSource->SetRadius(1.0);
    coneSource->SetResolution(50);


    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(coneSource->GetOutputPort());

    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.2, 0.6, 0.9);

    m_renderer->AddActor(actor);
    m_renderer->ResetCamera();
}
