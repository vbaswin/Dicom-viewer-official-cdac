#include <QDebug>
#include "vtkDICOMReader.h"
#include "vtkGPUVolumeRayCastMapper.h"
#include "vtkImageData.h"
#include "vtkSmartPointer.h"
#include <iostream>

const char *filefname = "C:/Users/cdac/Projects/SE2dcm";

vtkImageData *getImageData()
{
    vtkDICOMReader *reader = vtkSmartPointer<vtkDICOMReader>::New();
    return reader->GetOutput();
}

int main()
{
    // qDebug() << "hello";
}
