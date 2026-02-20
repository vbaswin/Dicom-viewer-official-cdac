#ifndef MIPVIEWER_H
#define MIPVIEWER_H

#include "vtkImageReslice.h"
#include "vtkNew.h"

enum class MipAxis {
    Sagittal = 0,
    Coronal = 1,
    Axial = 2,
};

class MipViewer
{
public:
    MipViewer() = default;
    ~MipViewer() = default;

    // Non-copyable — owns VTK pipeline objects with reference semantics.
    MipViewer(const MipViewer &) = delete;
    MipViewer &operator=(const MipViewer &) = delete;

    void setInputData(vtkImageData *data);

    // override the window/level
    void setWindowLevel(double window, double level);

    // Recompute and display the MIP for the given axis.
    [[nodiscard]] vtkImageData *viewMip(MipAxis axis = MipAxis::Sagittal);

private:
    vtkNew<vtkImageReslice> m_reslice;
    vtkImageData *m_imageData = nullptr;
};

#endif // MIPVIEWER_H
