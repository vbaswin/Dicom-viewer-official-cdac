 #include "mainwindow.h"

// VTK DICOM reader — from vtkDICOM 0.8.13 library.
// The reader itself handles slice sorting by ImagePositionPatient,
// so we do NOT need vtkDICOMDirectory or vtkDICOMFileSorter.
#include "vtkDICOMReader.h"

// VTK 2D image viewer — purpose-built for medical slice viewing.
// Internally manages: renderer, image actor, window/level lookup table.
#include "vtkImageViewer2.h"

// VTK interaction
#include "vtkRenderWindowInteractor.h"
#include "vtkInteractorStyleImage.h"
#include "vtkCallbackCommand.h"

// VTK utilities
#include "vtkStringArray.h"
#include "vtkRenderer.h"
#include "vtkCornerAnnotation.h"
#include "vtkTextProperty.h"

#include <QDebug>
#include <QDir>
#include <QStringList>

// ---------------------------------------------------------------------------
// Anonymous namespace for file-local helpers.
// Prevents global symbol pollution (the "Library Mindset").
// ---------------------------------------------------------------------------
namespace {

/// @brief Callback invoked on mouse wheel events to navigate DICOM slices.
///
/// VTK uses the Observer/Command pattern. The vtkImageViewer2 pointer
/// is passed via clientData — a standard VTK pattern for bridging
/// C-style callbacks to C++ objects.
void scrollCallback(vtkObject * /*caller*/, unsigned long eventId,
                    void *clientData, void * /*callData*/)
{
    auto *viewer = static_cast<vtkImageViewer2 *>(clientData);
    if (viewer == nullptr) {
        return;
    }

    const int currentSlice = viewer->GetSlice();
    const int minSlice     = viewer->GetSliceMin();
    const int maxSlice     = viewer->GetSliceMax();

    if (eventId == vtkCommand::MouseWheelForwardEvent) {
        viewer->SetSlice(std::min(currentSlice + 1, maxSlice));
    }
    else if (eventId == vtkCommand::MouseWheelBackwardEvent) {
        viewer->SetSlice(std::max(currentSlice - 1, minSlice));
    }

    viewer->Render();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// MainWindow Implementation
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("DICOM Viewer");
    resize(1024, 768);

    setupVTKWidget();

    // Load the DICOM dataset. The path is injected at the call site —
    // loadDicomDirectory() itself is path-agnostic (Dependency Inversion).
    loadDicomDirectory("C:/Users/aswin/OneDrive/Documents/dicom-images");
}

MainWindow::~MainWindow() = default;

void MainWindow::setupVTKWidget()
{
    // QVTKOpenGLNativeWidget is the modern VTK-Qt bridge (VTK 8.1+).
    // Passing 'this' makes Qt manage its lifetime via parent-child ownership.
    m_vtkWidget = new QVTKOpenGLNativeWidget(this);
    setCentralWidget(m_vtkWidget);

    m_vtkWidget->SetRenderWindow(m_renderWindow);
    m_renderWindow->GetInteractor()->Initialize();
}

void MainWindow::loadDicomDirectory(const QString &directoryPath)
{
    // -----------------------------------------------------------------------
    // Step 1: Validate directory exists.
    // Fail-fast: report the error immediately rather than crashing later
    // in the VTK pipeline with a cryptic message.
    // -----------------------------------------------------------------------
    const QDir dir(directoryPath);
    if (!dir.exists()) {
        qWarning() << "DICOM directory does not exist:" << directoryPath;
        setWindowTitle("DICOM Viewer — ERROR: Directory not found");
        return;
    }

    // -----------------------------------------------------------------------
    // Step 2: List all .dcm files using QDir.
    //
    // WHY QDir instead of vtkDICOMDirectory:
    //   - Zero additional VTK library dependencies
    //   - vtkDICOMReader handles slice sorting internally by reading
    //     ImagePositionPatient from each file's DICOM metadata
    //   - QDir is well-tested, Qt-native, and already linked
    // -----------------------------------------------------------------------
    const QStringList nameFilters = {"*.dcm"};
    const QStringList fileList = dir.entryList(nameFilters, QDir::Files, QDir::Name);

    if (fileList.isEmpty()) {
        qWarning() << "No .dcm files found in:" << directoryPath;
        setWindowTitle("DICOM Viewer — ERROR: No .dcm files found");
        return;
    }

    qDebug() << "Found" << fileList.size() << ".dcm files in" << directoryPath;

    // Convert Qt file list → VTK string array (absolute paths).
    vtkNew<vtkStringArray> vtkFileNames;
    for (const QString &fileName : fileList) {
        const QString absolutePath = dir.absoluteFilePath(fileName);
        vtkFileNames->InsertNextValue(absolutePath.toUtf8().constData());
    }

    // -----------------------------------------------------------------------
    // Step 3: Read the DICOM series with vtkDICOMReader.
    //
    // vtkDICOMReader (vtkDICOM 0.8.13) is superior to VTK's built-in
    // vtkDICOMImageReader because it:
    //   - Sorts slices by ImagePositionPatient automatically
    //   - Applies RescaleSlope/Intercept for Hounsfield units
    //   - Handles compressed transfer syntaxes
    //   - Provides full DICOM metadata access
    // -----------------------------------------------------------------------
    m_dicomReader = vtkSmartPointer<vtkDICOMReader>::New();
    m_dicomReader->SetFileNames(vtkFileNames);
    m_dicomReader->Update();

    // -----------------------------------------------------------------------
    // Step 4: Set up vtkImageViewer2 for 2D slice viewing.
    //
    // vtkImageViewer2 uses Composition internally — it creates and manages:
    //   - vtkImageMapToWindowLevelColors (brightness/contrast)
    //   - vtkImageActor (GPU-accelerated texture display)
    //   - vtkRenderer (2D orthographic camera)
    // One class delegates to many specialized objects behind a clean API.
    // -----------------------------------------------------------------------
    m_imageViewer = vtkSmartPointer<vtkImageViewer2>::New();
    m_imageViewer->SetInputConnection(m_dicomReader->GetOutputPort());

    // Use the same render window that our Qt widget owns.
    m_imageViewer->SetRenderWindow(m_renderWindow);
    m_imageViewer->SetupInteractor(m_renderWindow->GetInteractor());

    // Axial view (looking down the Z-axis: head-to-feet in CT).
    m_imageViewer->SetSliceOrientationToXY();

    // -----------------------------------------------------------------------
    // Step 5: Configure window/level for typical CT soft-tissue viewing.
    //
    // Window = range of Hounsfield values mapped to [black, white].
    // Level  = center of that range.
    // Defaults: W:400, L:40 → standard "soft tissue" preset.
    // User can adjust interactively via middle-mouse drag.
    // -----------------------------------------------------------------------
    m_imageViewer->SetColorWindow(400);
    m_imageViewer->SetColorLevel(40);

    // Cache slice range.
    m_minSlice = m_imageViewer->GetSliceMin();
    m_maxSlice = m_imageViewer->GetSliceMax();

    // Start at the middle slice — often the most anatomically informative.
    const int middleSlice = (m_minSlice + m_maxSlice) / 2;
    m_imageViewer->SetSlice(middleSlice);

    // -----------------------------------------------------------------------
    // Step 6: Add corner annotation overlay showing slice info.
    //
    // <slice> and <slice_max> are vtkCornerAnnotation format tags that
    // auto-update on each render pass.
    // -----------------------------------------------------------------------
    vtkNew<vtkCornerAnnotation> annotation;
    annotation->SetLinearFontScaleFactor(2);
    annotation->SetNonlinearFontScaleFactor(1);
    annotation->SetMaximumFontSize(18);
    annotation->SetText(0, "Slice: <slice> / <slice_max>");  // Bottom-left
    annotation->SetText(2, "DICOM Viewer");                   // Top-left
    annotation->GetTextProperty()->SetColor(1.0, 1.0, 1.0);
    m_imageViewer->GetRenderer()->AddViewProp(annotation);

    m_imageViewer->GetRenderer()->SetBackground(0.05, 0.05, 0.05);

    // -----------------------------------------------------------------------
    // Step 7: Wire up mouse wheel scrolling via Observer pattern.
    //
    // vtkCallbackCommand bridges VTK's event system to our free function.
    // The vtkImageViewer2 pointer is injected as clientData.
    // -----------------------------------------------------------------------
    vtkNew<vtkCallbackCommand> wheelCallback;
    wheelCallback->SetCallback(scrollCallback);
    wheelCallback->SetClientData(m_imageViewer);

    vtkRenderWindowInteractor *interactor = m_renderWindow->GetInteractor();
    interactor->AddObserver(vtkCommand::MouseWheelForwardEvent, wheelCallback);
    interactor->AddObserver(vtkCommand::MouseWheelBackwardEvent, wheelCallback);

    // -----------------------------------------------------------------------
    // Step 8: Initial render.
    // -----------------------------------------------------------------------
    m_imageViewer->Render();

    setWindowTitle(QString("DICOM Viewer — %1 slices [%2/%3]")
                       .arg(fileList.size())
                       .arg(middleSlice)
                       .arg(m_maxSlice));

    qDebug() << "DICOM loaded successfully."
             << "Slices:" << m_minSlice << "-" << m_maxSlice
             << "| Starting at:" << middleSlice;
}
