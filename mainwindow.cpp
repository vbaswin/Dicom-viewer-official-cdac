 #include "mainwindow.h"
#include "SphereInteractorStyle.h"

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

#include <QHBoxLayout>


#include "vtkSphereWidget.h"
#include "vtkCoordinate.h"
#include "vtkImageData.h"
#include "vtkProperty.h"
#include "vtkCamera.h"

// VTK utilities
#include "vtkStringArray.h"
#include "vtkRenderer.h"
#include "vtkCornerAnnotation.h"
#include "vtkTextProperty.h"
#include "vtkDICOMDirectory.h"

#include <QDebug>
#include <QToolBar>
#include <QDir>
#include <QStringList>

#include "vtkVolume.h" // 3d actor equivalent
#include "vtkVolumeProperty.h" // binds transfer functions
#include "vtkGPUVolumeRayCastMapper.h" // mip mapper


#include "vtkPiecewiseFunction.h" // opacity map
#include "vtkColorTransferFunction.h" // color ramp



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

    setupToolBar();
    setupVTKWidget();

    // Load the DICOM dataset. The path is injected at the call site —
    // loadDicomDirectory() itself is path-agnostic (Dependency Inversion).
    loadDicomDirectory("C:/Users/cdac/Projects/SE2dcm");
}

MainWindow::~MainWindow() = default;

void MainWindow::setupToolBar() {
    QToolBar *toolbar = addToolBar("Tools");

    m_annotateButton = new QPushButton("Mark Point", this);
    m_annotateButton->setCheckable(true);
    // m_annotateButton->setChecked(true);
    connect(m_annotateButton, &QPushButton::toggled,
            this, &MainWindow::toggleAnnotationMode);
    toolbar->addWidget(m_annotateButton);
}
void MainWindow::setupVTKWidget()
{
    QWidget *container = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0,0, 0, 0);
    layout->setSpacing(0);

    m_vtkWidget = new QVTKOpenGLNativeWidget(container);
    m_mipWidget = new QVTKOpenGLNativeWidget(container);

    layout->addWidget(m_vtkWidget, 1);
    layout->addWidget(m_mipWidget, 1);

    setCentralWidget(container);

    m_vtkWidget->SetRenderWindow(m_renderWindow);
    m_mipWidget->SetRenderWindow(m_mipRenderWindow);

    m_renderWindow->GetInteractor()->Initialize();
    m_mipRenderWindow->GetInteractor()->Initialize();
}

void MainWindow::toggleAnnotationMode(bool enabled)
{
    if (m_sphereStyle) {
        m_sphereStyle->SetAnnotationMode(enabled);
    }
}

void MainWindow::loadDicomDirectory(const QString &directoryPath)
{
    // -----------------------------------------------------------------------
    // Step 1: Scan the directory with vtkDICOMDirectory.
    //
    // WHY this over QDir:
    //   - Detects DICOM files by header magic bytes, not just ".dcm" extension
    //   - Automatically groups files into separate series (by SeriesInstanceUID)
    //   - Returns vtkStringArray directly — no Qt↔VTK string conversion needed
    //   - Sorts by ImagePositionPatient within each series
    //
    // SetScanDepth(1) means: scan only the given directory, not subdirectories.
    // Increase to 2+ if your DICOM files are nested in subfolders.
    // -----------------------------------------------------------------------
    vtkNew<vtkDICOMDirectory> dicomDir;
    dicomDir->SetDirectoryName(directoryPath.toUtf8().constData());
    dicomDir->SetScanDepth(1);
    dicomDir->Update();





    const int numberOfSeries = dicomDir->GetNumberOfSeries();
    if (numberOfSeries == 0) {
        qWarning() << "No DICOM series found in:" << directoryPath;
        setWindowTitle("DICOM Viewer — ERROR: No DICOM series found");
        return;
    }

    qDebug() << "Found" << numberOfSeries << "DICOM series";

    // -----------------------------------------------------------------------
    // Step 2: Get file names for the first series.
    //
    // GetFileNamesForSeries() returns a vtkStringArray* directly —
    // no QDir, no QStringList, no manual conversion loop.
    // If you have multiple series (e.g., CT + scout), you'd let the user
    // choose which series to load. For now, we take the first one.
    // -----------------------------------------------------------------------
    constexpr int seriesIndex = 0;  // First series
    vtkStringArray *fileNames = dicomDir->GetFileNamesForSeries(seriesIndex);

    if (fileNames == nullptr || fileNames->GetNumberOfValues() == 0) {
        qWarning() << "Series 0 contains no files";
        setWindowTitle("DICOM Viewer — ERROR: Empty series");
        return;
    }

    qDebug() << "Series 0 contains" << fileNames->GetNumberOfValues() << "files";

    // -----------------------------------------------------------------------
    // Step 3: Read the DICOM series.
    // -----------------------------------------------------------------------
    m_dicomReader = vtkSmartPointer<vtkDICOMReader>::New();
    m_dicomReader->SetFileNames(fileNames);  // ← Direct! No conversion!
    m_dicomReader->Update();

    double range[2];
    m_dicomReader->GetOutput()->GetScalarRange(range);


    vtkNew<vtkGPUVolumeRayCastMapper> mipMapper;
    mipMapper->SetInputConnection(m_dicomReader->GetOutputPort());
    mipMapper->SetBlendModeToMaximumIntensity();

        // 2. Transfer functions — for MIP, opacity is binary (all or nothing),
  //    color is a simple grayscale ramp over the actual data range.
    vtkNew<vtkPiecewiseFunction> opacity;
    opacity->AddPoint(range[0], 0.0);
    opacity->AddPoint(-500.0, 0.0);
    opacity->AddPoint(-499.0, 1.0);
    opacity->AddPoint(range[1], 1.0);

    vtkNew<vtkColorTransferFunction> color;
    color->AddRGBPoint(range[0], 0.0, 0.0, 0.0); // black at min
    color->AddRGBPoint(range[1], 1.0, 1.0, 1.0); // white at max

    vtkNew<vtkVolumeProperty> volProp;
    volProp->SetScalarOpacity(opacity);
    volProp->SetColor(color);
    volProp->ShadeOff(); // Shading is meaningless in MIP — max value always wins


    vtkNew<vtkVolume> mipVolume;
    mipVolume->SetMapper(mipMapper);
    mipVolume->SetProperty(volProp);

    vtkNew<vtkRenderer> mipRenderer;
    mipRenderer->AddVolume(mipVolume);


    vtkCamera *cam = mipRenderer->GetActiveCamera();
    cam->SetPosition(0, -1, 0);   // camera sits on -Y axis (anterior)
    cam->SetFocalPoint(0, 0, 0);  // looking toward +Y (posterior)
    cam->SetViewUp(0, 0, 1);      // Z = superior = "up" in coronal


    mipRenderer->ResetCamera();
    mipRenderer->SetBackground(0.05, 0.05, 0.05);

    m_mipRenderWindow->AddRenderer(mipRenderer);
    m_mipRenderWindow->Render();

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

    // create and install the custom style
    m_sphereStyle = vtkSmartPointer<SphereInteractorStyle>::New();
    m_sphereStyle->SetDefaultRenderer(m_imageViewer->GetRenderer());


    // Replace the default vtkInteractorStyleImage with ours.
    // Since SphereInteractorStyle IS-A vtkInteractorStyleImage (Liskov
    // Substitution), all existing image interaction (window/level, etc.)
    // continues to work — we only ADD behaviour on top.
    m_renderWindow->GetInteractor()->SetInteractorStyle(m_sphereStyle);

    // Sync: the button's toggled signal fired during setupToolBar(),
    // but m_sphereStyle didn't exist yet — push the state now.
    m_sphereStyle->SetAnnotationMode(m_annotateButton->isChecked());


    // Axial view (looking down the Z-axis: head-to-feet in CT).
    // m_imageViewer->SetSliceOrientationToXY();
    m_imageViewer->SetSliceOrientationToYZ();
    // m_imageViewer->SetSliceOrientationToXZ();

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



    const int  totalSlices = static_cast<int>(fileNames->GetNumberOfValues());
    const int   sliceStep = 5;
    m_sphereStyle->SetImageViewer(m_imageViewer, totalSlices, sliceStep);
    m_sphereStyle->SetSliceChangedCallback([this, totalSlices](int current, int maxSlice, int /*total*/) {
        setWindowTitle(QString("DICOM Viewer - %1 slices [%2/%3]")
                           .arg(totalSlices)
                           .arg(current)
                           .arg(maxSlice)
                       );
    });


    // Start at the middle slice — often the most anatomically informative.
    const int middleSlice = (m_minSlice + m_maxSlice) / 2;
    m_imageViewer->SetSlice(middleSlice);
    // m_imageViewer->SetSlice();
    m_imageViewer->GetRenderer()->ResetCamera();

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
    const std::string sliceLabel = "Slice: <slice> / " + std::to_string(totalSlices);
    annotation->SetText(0, sliceLabel.c_str());
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
    // vtkNew<vtkCallbackCommand> wheelCallback;
    // wheelCallback->SetCallback(scrollCallback);
    // wheelCallback->SetClientData(m_imageViewer);

    // vtkRenderWindowInteractor *interactor = m_renderWindow->GetInteractor();
    // interactor->AddObserver(vtkCommand::MouseWheelForwardEvent, wheelCallback);
    // interactor->AddObserver(vtkCommand::MouseWheelBackwardEvent, wheelCallback);

    // -----------------------------------------------------------------------
    // Step 8: Initial render.
    // -----------------------------------------------------------------------
    m_imageViewer->Render();

    setWindowTitle(QString("DICOM Viewer — %1 slices [%2/%3]")
                       .arg(fileNames->GetNumberOfValues())
                       .arg(middleSlice)
                       .arg(m_maxSlice));

    qDebug() << "DICOM loaded successfully."
             << "Slices:" << m_minSlice << "-" << m_maxSlice
             << "| Starting at:" << middleSlice;
}
