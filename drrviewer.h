#ifndef DRRVIEWER_H
#define DRRVIEWER_H

#include "vtkImageReslice.h"
#include "vtkNew.h"

enum class DrrAxis {
    Sagittal = 0,
    Coronal = 1,
    Axial = 2,
};

class DrrViewer
{
public:
    DrrViewer() = default;
    ~DrrViewer() = default;

    // Non-copyable — owns VTK pipeline objects with reference semantics.
    DrrViewer(const DrrViewer &) = delete;
    DrrViewer &operator=(const DrrViewer &) = delete;

    void setInputData(vtkImageData *data);

    // Recompute and display the MIP for the given axis.
    [[nodiscard]] vtkImageData *viewDrr(DrrAxis axis = DrrAxis::Sagittal);

private:
    vtkNew<vtkImageReslice> m_reslice;
    vtkImageData *m_imageData = nullptr;
};
#endif // DRRVIEWER_H
