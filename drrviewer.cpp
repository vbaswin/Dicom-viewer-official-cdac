#include "drrviewer.h"
#include "vtkCamera.h"
#include "vtkImageData.h"
#include "vtkImageMapper3D.h"
#include "vtkInteractorStyleImage.h"
#include "vtkMatrix4x4.h"

namespace Drr {

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

} // namespace Drr

void DrrViewer::setInputData(vtkImageData *data)
{
    m_imageData = data;

    /* ── Stage 1: HU → attenuation remapping ────────────────────────────────
    //
    // Raw CT HU values:  air=-1000, water=0, bone=+1000
    // In real X-ray physics, air is TRANSPARENT (μ=0), not negative.
    // Naively summing raw HU makes air contribute −1000 per slice,
    // pulling the entire DRR into massive negative values → unusable image.
    //
    // Fix: shift every voxel by +1000 BEFORE summing.
    //   air  (HU=−1000) → 0     ← transparent, contributes nothing ✓
    //   water(HU=    0) → 1000  ← baseline attenuation ✓
    //   bone (HU=+1000) → 2000  ← twice water, dense ✓
    //
    // This is the linearised Beer-Lambert remapping:
    //   μ ∝ (HU + 1000)
    */
    m_huRemap->SetInputData(data);
    m_huRemap->SetShift(1000.0);
    m_huRemap->SetScale(1.0);
    m_huRemap->SetOutputScalarTypeToFloat();

    m_reslice->SetInputConnection(m_huRemap->GetOutputPort());
    m_reslice->SetOutputScalarType(VTK_FLOAT);
}

vtkImageData *DrrViewer::viewDrr(DrrAxis axis)
{
    vtkImageData *vol = m_imageData;
    if (!vol) {
        return nullptr; // Fail fast — caller forgot setInputeonnection()
    }

    int dims[3];      // the voxel no in each axis
    double bounds[6]; // the max and min bounds in each axis
    vol->GetDimensions(dims);
    vol->GetBounds(bounds);

    // calculate center from each max and min bounds
    const double cx = (bounds[0] + bounds[1]) * 0.5;
    const double cy = (bounds[2] + bounds[3]) * 0.5;
    const double cz = (bounds[4] + bounds[5]) * 0.5;

    const auto &cfg = Drr::kAxisConfigs[static_cast<int>(axis)];

    vtkNew<vtkMatrix4x4> resliceAxes;
    resliceAxes->DeepCopy(cfg.matrix);
    // insierting center to the transilation column
    resliceAxes->SetElement(0, 3, cx);
    resliceAxes->SetElement(1, 3, cy);
    resliceAxes->SetElement(2, 3, cz);

    m_reslice->SetOutputDimensionality(2);
    m_reslice->SetResliceAxes(resliceAxes);
    m_reslice->SetInterpolationModeToLinear();
    // sum through voxels for each ray
    m_reslice->SetSlabModeToSum();
    // how many vosels deep the volume along slab axis
    m_reslice->SetSlabNumberOfSlices(dims[cfg.slabDimIdx]);
    m_reslice->Update();

    return m_reslice->GetOutput();
}
