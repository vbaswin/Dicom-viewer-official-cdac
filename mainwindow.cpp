#include "mainwindow.h"
// #include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    // , ui(new Ui::MainWindow)
{
    // ui->setupUi(this);
    setWindowTitle("DICOM Viewer");
    resize(1024, 768);
}

MainWindow::~MainWindow()
{
    // delete ui;
}
