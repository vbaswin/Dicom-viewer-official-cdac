
  This is a flat, single-module Qt/VTK C++ application for viewing DICOM medical images with a sphere
  annotation system. There are no existing documentation files (no README, no cursor rules, no copilot
   instructions). The project uses qmake as its build system with hardcoded dependency paths targeting
   a specific Windows machine.

  Location

  File: C:\Users\cdac\Projects\Dicom-Viewer\CLAUDE.md (new file — create at project root)

  Content

  # CLAUDE.md

  This file provides guidance to Claude Code (claude.ai/code) when working with code in this
  repository.

  ## Build

  This project uses **qmake** (Qt 5.15.2, MSVC 2019 x64). No CMake.

  **From Qt Creator:** Open `Dicom-Viewer.pro` with the `Desktop Qt 5.15.2 MSVC2019 64bit` kit, then
  build.

  **From command line** (requires Qt 5.15.2 + MSVC2019 environment):
  ```bat
  qmake Dicom-Viewer.pro
  nmake

  External Dependencies (pre-built, hardcoded paths in .pro)

  ┌──────────┬─────────┬─────────────────────────────────────────┐
  │ Library  │ Version │              Path in .pro               │
  ├──────────┼─────────┼─────────────────────────────────────────┤
  │ VTK      │ 8.2     │ C:\Users\cdac\Projects\VTK8.2\install   │
  ├──────────┼─────────┼─────────────────────────────────────────┤
  │ vtkDICOM │ 0.8.13  │ C:\Users\cdac\Projects\VTKDicom\install │
  └──────────┴─────────┴─────────────────────────────────────────┘

  There is no test framework or test infrastructure configured.

  Architecture

  Three logical components across four source files (flat structure, no subdirectories):

  MainWindow (mainwindow.h / mainwindow.cpp)

  - Central orchestrator. Owns the VTK rendering pipeline and Qt UI.
  - loadDicomDirectory(): uses vtkDICOMDirectory to scan a folder for DICOM series, reads via
  vtkDICOMReader, displays via vtkImageViewer2.
  - Hosts a QVTKOpenGLNativeWidget as the Qt↔VTK bridge.
  - Slice navigation is handled by a vtkCallbackCommand (free function scrollCallback in anonymous
  namespace) bound to mouse wheel events.
  - A toolbar QPushButton (m_annotateButton) toggles annotation mode on SphereInteractorStyle.

  SphereInteractorStyle (SphereInteractorStyle.h — header-only)

  - Extends vtkInteractorStyleImage. Custom interactor for placing and dragging sphere annotations.
  - State machine: When m_annotationMode is off, all input passes through to base class
  (pan/zoom/window-level). When on:
    - Click on empty space → createSphereAt() (alternates between sphereA and sphereB per pair).
    - Click on existing sphere → DRAG_SPHERE mode.
    - Click on cylinder → DRAG_CYLINDER mode (rigid translation of both endpoints).
  - Inner struct AnnotationPair: holds two sphere actors, a vtkLineSource, a vtkTubeFilter (cylinder
  geometry), and the cylinder actor.
  - Hard cap of 4 annotation pairs (8 spheres) via m_clickCount >= 8.

  Entry Point (main.cpp)

  - Minimal bootstrap: creates QApplication and MainWindow.

  Key Patterns

  - VTK smart pointers: All VTK objects use vtkSmartPointer/vtkNew for automatic lifetime management.
  - VTK module init macros (VTK_MODULE_INIT) are in mainwindow.h — these register VTK's
  OpenGL2/FreeType factories and must exist in exactly one translation unit.
  - Forward declarations in mainwindow.h to minimize header coupling.
  - Anonymous namespace for file-local free functions (e.g., scrollCallback).

  Known Technical Debt

  1. Hardcoded DICOM path in mainwindow.cpp: loadDicomDirectory("C:/Users/aswin/...") — points to a
  different user's machine.
  2. Hardcoded library paths in Dicom-Viewer.pro — not portable.
  3. SphereInteractorStyle is entirely header-only despite ~350 lines of implementation — should be
  split into .h/.cpp.
  4. VTK_MODULE_INIT in header — risks ODR violations if mainwindow.h is included in multiple .cpp
  files. Should move to mainwindow.cpp.
  5. Magic number m_clickCount >= 8 — should be a named constexpr.
  6. No namespace on SphereInteractorStyle — lives in global scope.
  7. No tests — zero test infrastructure.
  8. Commented-out GDCM block in .pro (lines 66-88) — dead configuration.


  ---

  **Note on the `VTK_MODULE_INIT` concern (#4):** Currently there is only one `.cpp` that includes
  `mainwindow.h` (`mainwindow.cpp`), so it works today. But the moment any other translation unit
  includes that header, you'll get linker errors from duplicate symbol definitions. Moving those three
   macros into `mainwindow.cpp` is a zero-risk fix.
