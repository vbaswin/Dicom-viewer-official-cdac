#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include "MipViewer.h"
#include "QVTKOpenGLNativeWidget.h"
#include "vtkActor.h"
#include "vtkGenericOpenGLRenderWindow.h"
#include "vtkNew.h"
#include "vtkRenderer.h"
#include "vtkSmartPointer.h"
#include <vtkCallbackCommand.h>

#include <vtkAutoInit.h>

// Core rendering
VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkInteractionStyle);
VTK_MODULE_INIT(vtkRenderingFreeType);


// Forward-declare VTK types to keep the header lightweight.
// Consumers of MainWindow don't need full VTK definitions — this is
// Interface Segregation in practice (only expose what's needed).
class vtkImageViewer2;
class vtkDICOMReader;
class vtkRenderWindowInteractor;
class SphereInteractorStyle;


// Volume rendering (CRITICAL)
VTK_MODULE_INIT(vtkRenderingVolumeOpenGL2);
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:

    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void loadDicomDirectory(const QString &directoryPath);
private slots:
    void toggleAnnotationMode(bool enabled);
private:
    std::unique_ptr<MipViewer> m_mipViewer;
    void setupVTKWidget();
    void setupToolBar();



    // --- UI Components ---
    QVTKOpenGLNativeWidget *m_vtkWidget = nullptr;  // Owned by Qt parent hierarchy
    QVTKOpenGLNativeWidget *m_mipWidget = nullptr;  // Owned by Qt parent hierarchy

    vtkNew<vtkRenderer> m_renderer;
    vtkNew<vtkGenericOpenGLRenderWindow> m_renderWindow;

    vtkSmartPointer<vtkImageViewer2> m_imageViewer;
    vtkSmartPointer<vtkDICOMReader> m_dicomReader;

    // state
    int m_minSlice = 0;
    int m_maxSlice = 0;

    vtkSmartPointer<SphereInteractorStyle> m_sphereStyle;

    QPushButton *m_annotateButton = nullptr;

    vtkNew<vtkGenericOpenGLRenderWindow> m_mipRenderWindow;
    QButtonGroup *m_mipAxisGroup = nullptr;
};
#endif // MAINWINDOW_H
