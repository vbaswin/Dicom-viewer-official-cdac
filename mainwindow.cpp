 #include "mainwindow.h"
#include "SphereInteractorStyle.h"
#include "vtkImageReslice.h"
#include "vtkImageReslice.h"     // 2D slab/MIP filter (vtkImagingCore — already linked)
#include "vtkImageActor.h"       // 2D image display actor (vtkRenderingImage — already linked)
#include "vtkMatrix4x4.h"       // 2D image display actor (vtkRenderingImage — already linked)
#include "vtkImageProperty.h"    // window/level control for vtkImageActor
#include "vtkImageMapToWindowLevelColors.h"

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

#include "vtkImageMapper3D.h"

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

void MainWindow::mpiViewer() {

    // using Image slicing
    if (!m_dicomReader || !m_dicomReader->GetOutput()) {
        qWarning() << "mpiViewerf; DICOM reader not initialized";
        return;
    }

    /* -------------------------------------------------------------------------
     Step 1: Query the volume's spatial geometry.

     WHY: We need:
       (a) dims[0]  → the number of voxels along X, which tells us how thick
                      the slab must be to project through the ENTIRE volume.
       (b) bounds[] → the world-space extents, from which we derive the center
                      point that anchors the reslice plane at the volume midpoint.
     -------------------------------------------------------------------------
        */
    vtkImageData *vol = m_dicomReader->GetOutput();
    int    dims[3];   // [Nx, Ny, Nz] — voxel count per axis
    double bounds[6]; // [xmin, xmax, ymin, ymax, zmin, zmax] — world space (mm)
    vol->GetDimensions(dims);
    vol->GetBounds(bounds);


    /* ----------------------
     * Center of the volume in world (mm) — the reslice plane pivots here.
     */
    const double cx = (bounds[0] + bounds[1]) * 0.5;
    const double cy = (bounds[2] + bounds[3]) * 0.5;
    const double cz = (bounds[4] + bounds[5]) * 0.5;

  /*-------------------------------------------------------------------------
     Step 2: Build the reslice axes matrix for a Sagittal MIP.

     vtkMatrix4x4 is a 4×4 matrix stored row-major: element(row, col).
     Its COLUMNS define the reslice coordinate frame in world space:

       Col 0 (output image X in world) = World Y = (0, 1, 0)  [A-P direction]
       Col 1 (output image Y in world) = World Z = (0, 0, 1)  [Superior = up]
       Col 2 (slab/projection axis)    = World X = (1, 0, 0)  [Left-Right axis]
       Col 3 (origin)                  = (cx, cy, cz)         [volume center]

     Written out as rows (row-major for DeepCopy):
       Row 0 = [ col0.x, col1.x, col2.x, origin.x ] = [ 0, 0, 1, cx ]
       Row 1 = [ col0.y, col1.y, col2.y, origin.y ] = [ 1, 0, 0, cy ]
       Row 2 = [ col0.z, col1.z, col2.z, origin.z ] = [ 0, 1, 0, cz ]
       Row 3 = [ 0,      0,      0,      1         ]

     Anatomy reference:
       The sagittal plane divides the body into Left (−X) and Right (+X).
       Projecting along X gives a side view: A-P (Y) as horizontal axis,
       Superior (Z) as vertical axis — head at top, feet at bottom.
     -------------------------------------------------------------------------
*/
    static const double sagittalMipElements[16] = {
        0, 0, 1, 0,   // Row 0 — origin column [0,3] set below
        1, 0, 0, 0,   // Row 1 — origin column [1,3] set below
        0, 1, 0, 0,   // Row 2 — origin column [2,3] set below
        0, 0, 0, 1    // Row 3 — homogeneous
    };

    /*
       Inject the volume center as the plane's world-space origin.
    Without this, the slab would be anchored at (0,0,0) and may miss the data.
    */
    vtkNew<vtkMatrix4x4> resliceAxes;
    resliceAxes->DeepCopy(sagittalMipElements);


    /*
    Inject the volume center as the plane's world-space origin.
    Without this, the slab would be anchored at (0,0,0) and may miss the data.
*/
    resliceAxes ->SetElement(0, 3, cx);
    resliceAxes ->SetElement(1, 3, cy);
    resliceAxes ->SetElement(2, 3, cz);

    /* Step 3: Configure vtkImageReslice for a true 2D MIP.
    //
    // SetSlabModeToMax()          → MIP: each output pixel = maximum voxel value
    //                               encountered along the full projection ray.
    //
    // SetSlabNumberOfSlices(N)    → N = dims[0] = full count of X voxels.
    //                               Each slab step ≈ 1 input voxel in X.
    //                               This ensures rays traverse the ENTIRE volume.
    //
    // SetOutputDimensionality(2)  → Output is a FLAT 2D vtkImageData (not a stack).
    //                               Combined with SetSlabModeToMax, this collapses
    //                               the slab into a single projection image.
    //
    // SetInterpolationModeToLinear() → Sub-voxel accuracy; reduces aliasing at
    //                               diagonal ray paths through the volume.
     */

    vtkNew<vtkImageReslice> reslice;
    reslice->SetInputConnection(m_dicomReader->GetOutputPort());
    reslice->SetOutputDimensionality(2);
    reslice->SetResliceAxes(resliceAxes);
    reslice->SetInterpolationModeToLinear();
    reslice->SetSlabModeToMax();
    reslice->SetSlabNumberOfSlices(dims[0]);
    reslice->Update();

    /* Step 4: Apply window/level via vtkImageMapToWindowLevelColors.
    //
    // WHY this instead of vtkImageProperty::SetColorWindow/Level:
    //   vtkImageProperty W/L relies on the GPU shader path and can silently
    //   fall back to auto-scaling the full scalar range when no explicit LUT
    //   is configured. With metal implants stretching the range to ~4000 HU,
    //   bone at 600 HU maps to only ~40% brightness — effectively invisible.
    //
    //   vtkImageMapToWindowLevelColors is a CPU pipeline filter that:
    //     1. Guaranteed to apply: runs before rendering, not during it.
    //     2. Outputs UCHAR [0,255]: display is always trivial and correct.
    //     3. Formula: output = clamp((S - (L - W/2)) / W, 0, 1) * 255
    //
    // Bone Window preset (clinical standard):
    //   W = 2000, L = 300  →  display range: [-700, +1300] HU
    //
    //   Air       (-1000 HU) → below window  → BLACK   (background invisible)
    //   Soft tissue  (50 HU) → 37.5%         → dark grey
    //   Cortical   (700  HU) → 70%           → clearly visible mid-grey ← BONE!
    //   Metal     (2500+ HU) → saturated     → WHITE   (implant visible)
    // -------------------------------------------------------------------------
        */


    vtkNew<vtkImageMapToWindowLevelColors> wlFilter;
    wlFilter->SetInputConnection(reslice->GetOutputPort());
    wlFilter->SetWindow(2000.0);
    wlFilter->SetLevel(300.0);
    wlFilter->Update();

    /* ------------------------------------------------------------------------
    // Step 5: Create a 2D renderer and force parallel (orthographic) projection.
    //
    // WHY ParallelProjectionOn():
    //   Perspective projection introduces depth distortion on a flat 2D image.
    //   Parallel projection is the medically correct display mode — pixel sizes
    //   are spatially accurate and the image appears undistorted.
    // -------------------------------------------------------------------------
*/
    vtkNew<vtkImageActor>mipActor;
    mipActor->GetMapper()->SetInputConnection(wlFilter->GetOutputPort());


    vtkNew<vtkRenderer> mipRenderer;
    mipRenderer->AddActor(mipActor);
    mipRenderer->SetBackground(0.05, 0.05, 0.05);
    mipRenderer->ResetCamera();
    mipRenderer->GetActiveCamera()->ParallelProjectionOn();


    m_mipRenderWindow->AddRenderer(mipRenderer);

    /*
            The default style (vtkInteractorStyleTrackballCamera) allows full 3D
         vtkInteractorStyleImage is the correct medical imaging 2D style
        */

    vtkNew<vtkInteractorStyleImage> mipStyle;
    m_mipRenderWindow->GetInteractor()->SetInteractorStyle(mipStyle);
    m_mipRenderWindow->Render();


    /* using Volume Rendering
    double range[2];
    m_dicomReader->GetOutput()->GetScalarRange(range);


    vtkNew<vtkGPUVolumeRayCastMapper> mipMapper;
    mipMapper->SetInputConnection(m_dicomReader->GetOutputPort());
    mipMapper->SetBlendModeToMaximumIntensity();

    vtkNew<vtkVolumeProperty> volProp;
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
*/

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

    mpiViewer();

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
