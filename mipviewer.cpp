#include "mipviewer.h"
#include "vtkCamera.h"
#include "vtkImageData.h"
#include "vtkImageMapper3D.h"
#include "vtkInteractorStyleImage.h"
#include "vtkMatrix4x4.h"
#include "vtkRenderWindowInteractor.h"

namespace {

/// @brief Reslice matrix + slab dimension for one projection axis.
/// matrix[]   - storing the current selected axis matrix
/// slabDimIdx - storing which axis the matrix belongs to
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

    // axis directions :
    // saggital x - to x +
    // coronal y - to y +
    // axial z - to z +

    // Sagittal
    // looking from  x- to x+ (projection) (1, 0, 0)
    // ouput image plane will be in the yz plane
    // output image horizontal axis y- to y+  (0, 1, 0)
    // output image vertical axis z- to z+  (0, 0 , 1)

    {{0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1}, 0},

    // coronal
    {{1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1}, 1},

    // axial
    {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, 2},
};

} // namespace

MipViewer::MipViewer(vtkGenericOpenGLRenderWindow *renderWindow)
    : m_renderWindow(renderWindow)
{
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
    vtkImageData *vol = vtkImageData::SafeDownCast(m_reslice->GetInput());
    if (!vol) {
        return; // Fail fast — caller forgot setInputConnection()
    }

    int dims[3];      // the voxel no in each axis
    double bounds[6]; // the max and min bounds in each axis
    vol->GetDimensions(dims);
    vol->GetBounds(bounds);

    // calculate center from each max and min bounds
    const double cx = (bounds[0] + bounds[1]) * 0.5;
    const double cy = (bounds[2] + bounds[3]) * 0.5;
    const double cz = (bounds[4] + bounds[5]) * 0.5;

    const auto &cfg = kAxisConfigs[static_cast<int>(axis)];

    vtkNew<vtkMatrix4x4> resliceAxes;
    resliceAxes->DeepCopy(cfg.matrix);
    // insierting center to the transilation column
    resliceAxes->SetElement(0, 3, cx);
    resliceAxes->SetElement(1, 3, cy);
    resliceAxes->SetElement(2, 3, cz);

    m_reslice->SetOutputDimensionality(2);
    m_reslice->SetResliceAxes(resliceAxes);
    m_reslice->SetInterpolationModeToLinear();
    // take max voxel along each ray
    m_reslice->SetSlabModeToMax();
    // how many vosels deep the volume along slab axis
    m_reslice->SetSlabNumberOfSlices(dims[cfg.slabDimIdx]);
    m_reslice->Update();

    m_renderer->ResetCamera();
    m_renderWindow->Render();
}
