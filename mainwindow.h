#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "QVTKOpenGLNativeWidget.h"
#include "vtkSmartPointer.h"
#include "vtkGenericOpenGLRenderWindow.h"
#include "vtkRenderer.h"
#include "vtkActor.h"
#include "vtkNew.h"



#include <vtkAutoInit.h>

// Core rendering
VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkInteractionStyle);
VTK_MODULE_INIT(vtkRenderingFreeType);

// Volume rendering (CRITICAL)
// VTK_MODULE_INIT(vtkRenderingVolumeOpenGL2);
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
private:
    void setupVTKWidget();
    void setupTestCone();

    QVTKOpenGLNativeWidget *m_vtkWidget;
    vtkNew<vtkRenderer> m_renderer;
    vtkNew<vtkGenericOpenGLRenderWindow> m_renderWindow;

};
#endif // MAINWINDOW_H
