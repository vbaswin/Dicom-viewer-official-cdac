#include <QApplication>
#include <QSurfaceFormat>
#include "QVTKOpenGLNativeWidget.h"
#include "mainwindow.h"
#include <Qdebug>

int main(int argc, char *argv[])

{
    qDebug() << "qdebug() 1";
    QApplication a(argc, argv);
    qDebug() << "qdebug() 2";



    qDebug() << "qdebug() 3";
    MainWindow w;
    qDebug() << "qdebug() 4";
    w.show();
    qDebug() << "qdebug() 5";

    return a.exec();
}
