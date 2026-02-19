#ifndef MIPVIEWER_H
#define MIPVIEWER_H

#include "vtkGenericOpenGLRenderWindow.h"
#include "vtkImageActor.h"
#include "vtkImageMapToWindowLevelColors.h"
#include "vtkImageReslice.h"
#include "vtkNew.h"
#include "vtkRenderer.h"

enum class MipAxis {
    Sagittal = 0,
    Coronal = 1,
    Axial = 2,
};

class MipViewer
{
public:
    explicit MipViewer(vtkGenericOpenGLRenderWindow *renderWindow);
    ~MipViewer() = default;

    // Non-copyable — owns VTK pipeline objects with reference semantics.
    MipViewer(const MipViewer &) = delete;
    MipViewer &operator=(const MipViewer &) = delete;

    void setInputData(vtkImageData *data);

    /// @brief Recompute and display the MIP for the given axis.
    void viewMip(MipAxis axis = MipAxis::Sagittal);

    /// @brief override the window/level
    void setWindowLevel(double window, double level);

private:
    vtkGenericOpenGLRenderWindow *m_renderWindow = nullptr;

    vtkNew<vtkImageReslice> m_reslice;
    vtkNew<vtkImageMapToWindowLevelColors> m_wlFilter;
    vtkNew<vtkImageActor> m_mipActor;
    vtkNew<vtkRenderer> m_renderer;
    vtkImageData *m_imageData = nullptr;

    double m_window = 2000.0;
    double m_level = 300.0;
};

#endif // MIPVIEWER_H
