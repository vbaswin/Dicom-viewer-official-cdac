#include <QApplication>
#include <QSurfaceFormat>
#include "QVTKOpenGLNativeWidget.h"
#include "mainwindow.h"
#include <Qdebug>

int main(int argc, char *argv[])

{
    QApplication a(argc, argv);



    MainWindow w;
    w.show();

    return a.exec();
}
