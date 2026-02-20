#include "mainwindow.h"
#include "SphereInteractorStyle.h"
#include "vtkImageMapToWindowLevelColors.h"
#include "vtkImageProperty.h" // window/level control for vtkImageActor
#include "vtkImageReslice.h"  // 2D slab/MIP filter (vtkImagingCore — already linked)
#include "vtkMatrix4x4.h"     // 2D image display actor (vtkRenderingImage — already linked)

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

#include "MipViewer.h"
#include "vtkCamera.h"
#include "vtkCoordinate.h"
#include "vtkImageData.h"
#include "vtkProperty.h"
#include "vtkSphereWidget.h"

// VTK utilities
#include "vtkStringArray.h"
#include "vtkRenderer.h"
#include "vtkCornerAnnotation.h"
#include "vtkTextProperty.h"
#include "vtkDICOMDirectory.h"

#include <QButtonGroup>
#include <QDebug>
#include <QDir>
#include <QStringList>
#include <QToolBar>
#include <array>

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
/// @brief Per-axis configuration for vtkImageReslice sagittal/coronal/axial MIP.
///
/// matrix[]    — row-major 4×4 reslice axes. The origin column ([*][3]) is
///               intentionally zero here; the volume centre is injected at
///               runtime so this stays constexpr.
/// slabDimIdx  — which entry of vtkImageData::GetDimensions() gives the
///               number of voxels to traverse along the projection ray.
///               (The slab must span the full volume depth on that axis.)
struct MipAxisConfig
{
    double matrix[16];
    int slabDimIdx;
};
//  Column layout of each matrix (VTK convention):
//    col 0 = direction in world space that maps to output image X-axis
//    col 1 = direction in world space that maps to output image Y-axis
//    col 2 = projection / slab direction (rays fire along this axis)
//    col 3 = origin (filled at runtime — zeros here)
//
//  Written as rows (row-major storage):
//    Row i = [ col0[i], col1[i], col2[i], 0 ]
static constexpr MipAxisConfig kAxisConfigs[] = {

    // ── MipAxis::Sagittal (index 0) ──────────────────────────────────────
    // Project along World X (Left↔Right).
    //   col0 (out-X) = (0,1,0) = World Y   ← anterior-posterior = horizontal
    //   col1 (out-Y) = (0,0,1) = World Z   ← superior = up (head at top)
    //   col2 (proj)  = (1,0,0) = World X
    {
        {0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1},
        0 // slab depth = dims[0]
    },

    // ── MipAxis::Coronal (index 1) ───────────────────────────────────────
    // Project along World Y (Anterior↔Posterior).
    //   col0 (out-X) = (1,0,0) = World X   ← left-right = horizontal
    //   col1 (out-Y) = (0,0,1) = World Z   ← superior = up
    //   col2 (proj)  = (0,1,0) = World Y
    {
        {1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1},
        1 // slab depth = dims[1]
    },

    // ── MipAxis::Axial (index 2) ─────────────────────────────────────────
    // Project along World Z (Superior↔Inferior). Identity rotation.
    //   col0 (out-X) = (1,0,0) = World X   ← left-right = horizontal
    //   col1 (out-Y) = (0,1,0) = World Y   ← anterior = up
    //   col2 (proj)  = (0,0,1) = World Z
    {
        {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
        2 // slab depth = dims[2]
    },
};

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

    toolbar->addSeparator();

    m_mipAxisGroup = new QButtonGroup(this);
    m_mipAxisGroup->setExclusive(true);

    const std::array<std::pair<MipAxis, QString>, 3> kAxes = {{
        {MipAxis::Sagittal, "Sagittal"},
        {MipAxis::Coronal, "Coronal"},
        {MipAxis::Axial, "Axial"},

    }};

    for (const auto &[axis, label] : kAxes) {
        auto *btn = new QPushButton(label, this);
        btn->setCheckable(true);
        toolbar->addWidget(btn);

        m_mipAxisGroup->addButton(btn, static_cast<int>(axis));
    }

    m_mipAxisGroup->button(static_cast<int>(MipAxis::Sagittal))->setChecked(true);

    connect(m_mipAxisGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_mipData = m_mipViewer->viewMip(static_cast<MipAxis>(id));
        if (m_mipData) {
            m_mipImageViewer->SetInputData(m_mipData);
            m_mipImageViewer->GetRenderer()->ResetCamera();
            m_mipImageViewer->Render();
        }
    });
}

void MainWindow::onMipWindowLevel(vtkObject *caller,
                                  unsigned long eventId,
                                  void *clientData,
                                  void *callData)
{
    auto *self = static_cast<MainWindow *>(clientData);
    if (!self->m_mipRenderWindow)
        return;

    const int w = static_cast<int>(self->m_mipImageViewer->GetColorWindow());
    const int l = static_cast<int>(self->m_mipImageViewer->GetColorLevel());

    const std::string text = "W: " + std::to_string(w) + " L: " + std::to_string(l);
    self->m_mipAnnotation->SetText(3, text.c_str());
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

    // m_mipImageViewer->SetRenderWindow(m_mipRenderWindow);

    m_renderWindow->GetInteractor()->Initialize();
    m_mipRenderWindow->GetInteractor()->Initialize();

    m_mipViewer = std::make_unique<MipViewer>();
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

    // ---------------------------------------------------------------------------------m
    m_mipViewer->setInputData(m_dicomReader->GetOutput());

    vtkImageData *m_mipData = m_mipViewer->viewMip();
    if (m_mipData) {
        m_mipImageViewer = vtkSmartPointer<vtkImageViewer2>::New();

        m_mipImageViewer->SetInputData(m_mipData);
        m_mipImageViewer->SetRenderWindow(m_mipRenderWindow);
        m_mipImageViewer->SetupInteractor(m_mipRenderWindow->GetInteractor());

        // annotation settings

        m_mipAnnotation->SetLinearFontScaleFactor(2);
        m_mipAnnotation->SetNonlinearFontScaleFactor(1);
        m_mipAnnotation->SetMaximumFontSize(16);
        m_mipAnnotation->GetTextProperty()->SetColor(1.0, 1.0, 0.0);

        m_mipAnnotation->SetText(3, "W: 2000 L: 400");
        m_mipImageViewer->GetRenderer()->AddViewProp(m_mipAnnotation);

        vtkInteractorStyleImage *mipStyle = vtkInteractorStyleImage::SafeDownCast(
            m_mipRenderWindow->GetInteractor()->GetInteractorStyle());

        if (mipStyle) {
            vtkNew<vtkCallbackCommand> wlCallback;
            wlCallback->SetCallback(MainWindow::onMipWindowLevel);
            wlCallback->SetClientData(this);
            mipStyle->AddObserver(vtkCommand::WindowLevelEvent, wlCallback);
        }

        m_mipImageViewer->SetColorWindow(2000.0);
        m_mipImageViewer->SetColorLevel(300.0);
        // m_mipImageViewer->SetColorWindow(1000.0);
        // m_mipImageViewer->SetColorLevel(400.0);

        m_mipImageViewer->Render();
        m_mipImageViewer->GetRenderer()->ResetCamera();
    }

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
