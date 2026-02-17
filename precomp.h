  // precomp.h — Stable Precompiled Header
//
// WHY: VTK headers are template-heavy and take the majority of compile time.
// By listing all stable third-party headers here, MSVC compiles them ONCE
// to a binary .pch file and never re-parses them again — even across rebuilds.
    //
    // RULE: Only put headers here that change NEVER (third-party/Qt/STL).
    // Never put your own project headers (mainwindow.h, SphereInteractorStyle.h) here.

#pragma once

// -----------------------------------------------------------------------
// Qt — stable, never change between your own edits
// -----------------------------------------------------------------------
#include <QMainWindow>
#include <QPushButton>
#include <QToolBar>
#include <QDir>
#include <QStringList>
#include <QDebug>
#include <QSurfaceFormat>

// -----------------------------------------------------------------------
// VTK Core
// -----------------------------------------------------------------------
#include "QVTKOpenGLNativeWidget.h"
#include "vtkSmartPointer.h"
#include "vtkNew.h"
#include "vtkGenericOpenGLRenderWindow.h"
#include "vtkRenderWindow.h"
#include "vtkRenderer.h"
#include "vtkActor.h"
#include "vtkCallbackCommand.h"
#include "vtkAutoInit.h"

// -----------------------------------------------------------------------
// VTK Interaction
// -----------------------------------------------------------------------
#include "vtkInteractorStyleImage.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkObjectFactory.h"

// -----------------------------------------------------------------------
// VTK Filters & Sources
// -----------------------------------------------------------------------
#include "vtkLineSource.h"
#include "vtkSphereSource.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkPropPicker.h"
#include "vtkCoordinate.h"
#include "vtkTubeFilter.h"
#include "vtkSphereWidget.h"

// -----------------------------------------------------------------------
// VTK Imaging & Annotation
// -----------------------------------------------------------------------
#include "vtkImageViewer2.h"
#include "vtkStringArray.h"
#include "vtkCornerAnnotation.h"
#include "vtkTextProperty.h"

// -----------------------------------------------------------------------
// VTK DICOM
// -----------------------------------------------------------------------
#include "vtkDICOMReader.h"
#include "vtkDICOMDirectory.h"

// -----------------------------------------------------------------------
// STL
// -----------------------------------------------------------------------
#include <vector>
#include <algorithm>
