#include <QDebug>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>
#include "QVTKOpenGLWidget.h"
#include "mainwindow.h"
#include <iostream>
#include <qapplication.h>

#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkInteractionStyle);
VTK_MODULE_INIT(vtkRenderingFreeType);
VTK_MODULE_INIT(vtkRenderingVolumeOpenGL2);

int main(int argc, char *argv[])
{
    // Essential for vtkGPUVolumeRayCastMapper when using QVTKOpenGLNativeWidget:
    // 1. Disable Multi-Sampling (MSAA) which interferes with VTK's offscreen volume rendering passes.
    QSurfaceFormat format = QVTKOpenGLNativeWidget::defaultFormat();
    format.setSamples(0);
    QSurfaceFormat::setDefaultFormat(format);

    // 2. Share OpenGL contexts across the application to ensure VTK shaders and textures load properly.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}
