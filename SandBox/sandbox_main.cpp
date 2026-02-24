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
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}
