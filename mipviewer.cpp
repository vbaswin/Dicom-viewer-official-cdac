#include "mipviewer.h"
#include "vtkCamera.h"
#include "vtkImageData.h"
#include "vtkImageMapper3D.h" // required to call GetMapper()->SetInputConnection()
#include "vtkInteractorStyleImage.h"
#include "vtkMatrix4x4.h"
#include "vtkRenderWindowInteractor.h"

namespace {

/// @brief Reslice matrix + slab dimension for one projection axis.
///
/// matrix[]   — row-major 4×4. Origin column ([*][3]) is zero here;
///              the volume centre is injected at runtime in viewMip().
/// slabDimIdx — index into vtkImageData::GetDimensions() that gives
///              the voxel count along the projection ray.
struct AxisConfig
{
    double matrix[16];
    int slabDimIdx;
};

// Column layout (VTK reslice axes convention):
//   col 0 = world direction → output image X-axis
//   col 1 = world direction → output image Y-axis
//   col 2 = projection / slab direction (rays fire along this)
//   col 3 = origin (filled at runtime)
static constexpr AxisConfig kAxisConfigs[] = {

    // MipAxis::Sagittal — project along World X (Left↔Right)
    //   out-X = World Y (A-P),  out-Y = World Z (S-I, head at top)
    {{0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1}, 0},

    // MipAxis::Coronal — project along World Y (Ant↔Post)
    //   out-X = World X (L-R),  out-Y = World Z (S-I, head at top)
    {{1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1}, 1},

    // MipAxis::Axial — project along World Z (Sup↔Inf), identity rotation
    //   out-X = World X (L-R),  out-Y = World Y (A-P)
    {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, 2},
};

} // namespace

MipViewer::MipViewer(vtkGenericOpenGLRenderWindow *renderWindow)
    : m_renderWindow(renderWindow)
{
    // Wire the static pipeline once.
    // VTK is lazy — no data flows until Update() or Render() is called,
    // so connecting before SetInputConnection() is set is perfectly safe.
    m_wlFilter->SetInputConnection(m_reslice->GetOutputPort());
    m_wlFilter->SetWindow(m_window);
    m_wlFilter->SetLevel(m_level);

    m_mipActor->GetMapper()->SetInputConnection(m_wlFilter->GetOutputPort());

    m_renderer->AddActor(m_mipActor);
    m_renderer->SetBackground(0.05, 0.05, 0.05);
    // Parallel projection is mandatory for 2D medical images —
    // perspective would introduce depth distortion on a flat plane.
    m_renderer->GetActiveCamera()->ParallelProjectionOn();

    m_renderWindow->AddRenderer(m_renderer);

    // Lock this panel to 2D image interaction.
    // vtkInteractorStyleImage: pan + zoom only, NO rotation.
    // The SetInteractorStyle() call increments the style's VTK refcount,
    // so the local vtkNew going out of scope here does not delete it.
    vtkNew<vtkInteractorStyleImage> style;
    m_renderWindow->GetInteractor()->SetInteractorStyle(style);
}

void MipViewer::setInputData(vtkImageData *data)
{
    m_imageData = data;
    m_reslice->SetInputData(data);
}

void MipViewer::setWindowLevel(double window, double level)
{
    m_window = window;
    m_level = level;
    m_wlFilter->SetWindow(m_window);
    m_wlFilter->SetLevel(m_level);
}

void MipViewer::viewMip(MipAxis axis)
{
    // GetInput() returns the upstream vtkImageData after the pipeline
    // has been connected. Safe to call here because setInputConnection()
    // must have been called before viewMip().
    vtkImageData *vol = vtkImageData::SafeDownCast(m_reslice->GetInput());
    if (!vol) {
        return; // Fail fast — caller forgot setInputConnection()
    }

    int dims[3];
    double bounds[6];
    vol->GetDimensions(dims);
    vol->GetBounds(bounds);

    const double cx = (bounds[0] + bounds[1]) * 0.5;
    const double cy = (bounds[2] + bounds[3]) * 0.5;
    const double cz = (bounds[4] + bounds[5]) * 0.5;

    const auto &cfg = kAxisConfigs[static_cast<int>(axis)];

    vtkNew<vtkMatrix4x4> resliceAxes;
    resliceAxes->DeepCopy(cfg.matrix);
    resliceAxes->SetElement(0, 3, cx); // inject volume centre
    resliceAxes->SetElement(1, 3, cy);
    resliceAxes->SetElement(2, 3, cz);

    m_reslice->SetOutputDimensionality(2);
    m_reslice->SetResliceAxes(resliceAxes);
    m_reslice->SetInterpolationModeToLinear();
    m_reslice->SetSlabModeToMax();
    m_reslice->SetSlabNumberOfSlices(dims[cfg.slabDimIdx]);
    m_reslice->Update();

    m_renderer->ResetCamera();
    m_renderWindow->Render();
}
