#ifndef SPHEREINTERACTORSTYLE_H
#define SPHEREINTERACTORSTYLE_H

#include "vtkRenderWindow.h"
#include <vtkInteractorStyleImage.h>
#include <vtkObjectFactory.h>          // vtkStandardNewMacro
#include <vtkLineSource.h>          // vtkStandardNewMacro
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSphereSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkPropPicker.h>
#include <vtkCoordinate.h>
#include <vtkSmartPointer.h>
#include <vtkTubeFilter.h>
#include "mainwindow.h"

#include <vector>
#include <vtkImageViewer2.h>
#include <functional>
#include <algorithm>


class SphereInteractorStyle : public vtkInteractorStyleImage {
public:
    static SphereInteractorStyle *New();
    vtkTypeMacro(SphereInteractorStyle, vtkInteractorStyleImage);

    void SetAnnotationMode(bool enabled) { m_annotationMode = enabled; }
    bool GetAnnotationMode() const { return m_annotationMode; }


    void OnLeftButtonDown() override {
        if (!m_annotationMode) {
            // normal image interactin
            vtkInteractorStyleImage::OnLeftButtonDown();
            return;
        }

        int *clickPos = this->Interactor->GetEventPosition();
        this->FindPokedRenderer(clickPos[0], clickPos[1]);
        if (!this->CurrentRenderer) return;
        vtkSmartPointer <vtkPropPicker> picker = vtkSmartPointer<vtkPropPicker>::New();

        // pick at 2D display , z depth always 0
        picker->Pick(clickPos[0], clickPos[1], 0, this->CurrentRenderer);

        vtkActor *pickedActor = dynamic_cast<vtkActor *>(picker->GetViewProp());

        int pairIdx = -1;;
        if (pickedActor && findPairIndexBySphere(pickedActor, pairIdx)) {
            m_dragMode = DRAG_SPHERE;
            m_draggedActor = pickedActor;
            m_dragPairIndex = pairIdx;


            // Highlight the picked sphere (visual feedback)
            // Concept: we change the actor's Property â€” the mapper/source
            // stay untouched (Composition: actor appearance is separate
            // from geometry).
            m_draggedActor->GetProperty()->SetColor(1.0, 1.0, 0.0); // yellow

            // Record offset so the sphere doesn't "jump" to cursor center.
            // offset = clickWorldPos - actorCurrentPos
            double worldPos[3];
            displayToWorld(clickPos[0], clickPos[1], worldPos);


            double *actorPos = m_draggedActor->GetPosition();
            m_dragOffset[0] = worldPos[0] - actorPos[0];
            m_dragOffset[1] = worldPos[1] - actorPos[1];
            m_dragOffset[2] = worldPos[2] - actorPos[2];


            // don't call parent, we are consuming this event
            return;
        }
    // clicked on one of cylinders
        if (pickedActor && findPairIndexByCylinder(pickedActor, pairIdx)) {
            m_dragMode = DRAG_CYLINDER;
            m_draggedActor = pickedActor;
            m_dragPairIndex = pairIdx;


            m_draggedActor->GetProperty()->SetColor(1.0, 1.0, 0.0); // yellow

            // Cache initial state so we can compute deltas during drag.
            // We store the mouse world pos AND both sphere positions at
            // the moment of click. On every MouseMove we compute:
            //   delta = currentMouse - initialMouse
            //   newSphereA = initialA + delta
            //   newSphereB = initialB + delta

            double worldPos[3];
            displayToWorld(clickPos[0], clickPos[1], worldPos);
            m_initialMouseWorld[0] = worldPos[0];
            m_initialMouseWorld[1] = worldPos[1];
            m_initialMouseWorld[2] = worldPos[2];


            AnnotationPair &pair = m_pairs[pairIdx];
            double *posA = pair.sphereA->GetPosition();
            double *posB = pair.sphereB->GetPosition();
            for (int i = 0; i < 3; ++i) {
                m_initialSphereAPos[i] = posA[i];
                m_initialSphereBPos[i] = posB[i];
            }
            return;
        }
        if (m_clickCount >= 8) {
            return;
        }

        // emtpy space - create a new sphere
        double worldPos[3];
        displayToWorld(clickPos[0], clickPos[1], worldPos);
        createSphereAt(worldPos);
    }
    // on move - update position if dragging

    void OnMouseMove() override {
        if (m_dragMode == DRAG_SPHERE ) {
          int *movePos = this->Interactor->GetEventPosition();
            double worldPos[3];
            displayToWorld(movePos[0], movePos[1], worldPos);

            // newActorPos - mouseWorldPos - offset
            // This keeps the grab-point stable under the cursor
            m_draggedActor->SetPosition(
                worldPos[0] - m_dragOffset[0],
                worldPos[1] - m_dragOffset[1],
                worldPos[2] - m_dragOffset[2]
                );

            AnnotationPair &pair = m_pairs[m_dragPairIndex];
            if (pair.lineSource) {
                updateCylinder(pair);
            }


            // reques re-render so user sees the sphere move in real-time
            this->Interactor->GetRenderWindow()->Render();
            return; // Don't pass to parent

        }
            if (m_dragMode == DRAG_CYLINDER) {
                int *movePos = this->Interactor->GetEventPosition();
                double worldPos[3];
                displayToWorld(movePos[0], movePos[1], worldPos);

                // Delta = how far mouse moved since initial click
                double delta[3];
                delta[0] = worldPos[0] - m_initialMouseWorld[0];
                delta[1] = worldPos[1] - m_initialMouseWorld[1];
                delta[2] = worldPos[2] - m_initialMouseWorld[2];

                // Move BOTH spheres by the same delta â†’ rigid body motion
                AnnotationPair &pair = m_pairs[m_dragPairIndex];
                pair.sphereA->SetPosition(
                    m_initialSphereAPos[0] + delta[0],
                    m_initialSphereAPos[1] + delta[1],
                    m_initialSphereAPos[2] + delta[2]
                    );
                pair.sphereB->SetPosition(
                    m_initialSphereBPos[0] + delta[0],
                    m_initialSphereBPos[1] + delta[1],
                    m_initialSphereBPos[2] + delta[2]
                    );

                updateCylinder(pair);
                this->Interactor->GetRenderWindow()->Render();
                return;
            }
    }

    // ===================================================================
    // LEFT BUTTON UP â€” end any active drag
    // ===================================================================
    void OnLeftButtonUp() override
    {
        if (m_dragMode == DRAG_SPHERE) {
            // Restore red
            m_draggedActor->GetProperty()->SetColor(1.0, 0.2, 0.2);
            resetDragState();
            this->Interactor->GetRenderWindow()->Render();
            return;
        }


        if (m_dragMode == DRAG_CYLINDER) {
            // Restore blue
            m_draggedActor->GetProperty()->SetColor(0.3, 0.6, 1.0);
            resetDragState();
            this->Interactor->GetRenderWindow()->Render();
            return;
        }

        vtkInteractorStyleImage::OnLeftButtonUp();
    }
    void OnMouseWheelForward () override {
        if (!m_viewer) return;
        m_viewer->SetSlice(std::min(m_viewer->GetSlice() + m_sliceStep, m_maxSlice));
        m_viewer->Render();
        if(m_sliceChangedCb)
            m_sliceChangedCb(m_viewer->GetSlice(), m_maxSlice, m_totalSlices);

        }

   void OnMouseWheelBackward () override {
       if (!m_viewer) return;
       m_viewer->SetSlice(std::max(m_viewer->GetSlice() - m_sliceStep, m_minSlice));
       m_viewer->Render();
       if(m_sliceChangedCb)
           m_sliceChangedCb(m_viewer->GetSlice(), m_maxSlice, m_totalSlices);


        }
   void SetImageViewer(vtkImageViewer2 *viewer, int totalSlices, int sliceStep) {
       m_viewer = viewer;
       m_totalSlices = totalSlices;
       m_minSlice = viewer->GetSliceMin();
       m_maxSlice = viewer->GetSliceMax();
       m_sliceStep = sliceStep;
   }
   void SetSliceChangedCallback(std::function<void(int, int, int)> cb) {
       m_sliceChangedCb = std::move(cb);
   }


private:
   vtkImageViewer2 *m_viewer = nullptr;
    int m_totalSlices = 0;
   int m_minSlice = 0;
    int m_maxSlice = 0;
    int m_sliceStep = 1;
   std::function<void(int, int, int)> m_sliceChangedCb;
    struct AnnotationPair {
        vtkSmartPointer<vtkActor> sphereA;
        vtkSmartPointer<vtkActor> sphereB;
        vtkSmartPointer<vtkLineSource> lineSource;
        vtkSmartPointer<vtkTubeFilter> tubeFilter;
        vtkSmartPointer<vtkActor> cylinderActor;

    };
    // sphere creation
    void createSphereAt(const double worldPos[3]) {
        vtkSmartPointer<vtkSphereSource> sphereSource = vtkSmartPointer<vtkSphereSource>::New();
        sphereSource->SetRadius(8.0);
        sphereSource->SetThetaResolution(20);
        sphereSource->SetPhiResolution(20);

        // source stays at 0,0,0, we move actor instead
        sphereSource->Update();


        // mapper geometry to gpu
        vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();


        mapper->SetInputConnection(sphereSource->GetOutputPort());

        // actor
        vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->SetPosition(worldPos[0], worldPos[1], worldPos[2]);
        actor->GetProperty()->SetColor(1.0, 0.2, 0.2);
        actor->GetProperty()->SetOpacity(0.8);


        // add to renderer, track in our list
        this->CurrentRenderer->AddActor(actor);

        if (m_clickCount % 2 == 0) {
            AnnotationPair newPair;
            newPair.sphereA = actor;
            m_pairs.push_back(newPair);
        }  else {
            AnnotationPair &pair = m_pairs.back();
            pair.sphereB = actor;
            createConnectionBetween(pair);

        }
        m_clickCount++;
        this->Interactor->GetRenderWindow()->Render();
    }

    void createConnectionBetween(AnnotationPair &pair) {
        double *posA = pair.sphereA->GetPosition();
        double *posB = pair.sphereB->GetPosition();


        // source
        pair.lineSource = vtkSmartPointer<vtkLineSource>::New();
        pair.lineSource->SetPoint1(posA);
        pair.lineSource->SetPoint2(posB);
        pair.lineSource->Update();

        // wrap a tube filter around it
        pair.tubeFilter = vtkSmartPointer<vtkTubeFilter>::New();
        pair.tubeFilter->SetInputConnection(pair.lineSource->GetOutputPort());
        pair.tubeFilter->SetRadius(1.5);
        pair.tubeFilter->SetNumberOfSides(20);
        pair.tubeFilter->CappingOn();
        pair.tubeFilter->Update();


        vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(pair.tubeFilter->GetOutputPort());

        pair.cylinderActor = vtkSmartPointer<vtkActor>::New();
        pair.cylinderActor->SetMapper(mapper);
        pair.cylinderActor->GetProperty()->SetColor(0.3, 0.6, 1.0);
        pair.cylinderActor->GetProperty()->SetOpacity(0.7);

        this->CurrentRenderer->AddActor(pair.cylinderActor);

    }

    void updateCylinder(AnnotationPair &pair) {
        double *posA = pair.sphereA->GetPosition();
        double *posB = pair.sphereB->GetPosition();

        pair.lineSource->SetPoint1(posA);
        pair.lineSource->SetPoint2(posB);
        pair.lineSource->Update();
    }

    bool findPairIndexBySphere(vtkActor *actor, int &outIndex) const {
        for (int i = 0; i < static_cast<int>(m_pairs.size()); ++i) {
            if (m_pairs[i].sphereA.GetPointer() == actor || (m_pairs[i].sphereB && m_pairs[i].sphereB.GetPointer() == actor)) {
                outIndex = i;
                return true;
            }
        }
        return false;
    }
    bool findPairIndexByCylinder(vtkActor *actor, int &outIndex) const {
        for (int i = 0; i < static_cast<int>(m_pairs.size()); ++i) {
            if (m_pairs[i].cylinderActor && m_pairs[i].cylinderActor.GetPointer() == actor) {
                outIndex = i;
                return true;
            }
        }

        return false;
    }

    // coordinatee conversin from dispaly (pixel) -> world (3D)
    void displayToWorld(int displayX, int displayY, double worldOut[3]) {
        vtkSmartPointer<vtkCoordinate> coord = vtkSmartPointer<vtkCoordinate>::New();
        coord->SetCoordinateSystemToDisplay();
        coord->SetValue(displayX, displayY, 0);


        double *w  = coord->GetComputedWorldValue(this->CurrentRenderer);
        worldOut[0] = w[0];
        worldOut[1] = w[1];
        worldOut[2] = w[2];

    }
    // state
    bool m_annotationMode = false;

    // std::vector<vtkSmartPointer<vtkActor>> m_sphereActors;
    std::vector<AnnotationPair> m_pairs;
    int m_clickCount = 0;

    enum DragMode { DRAG_NONE, DRAG_SPHERE, DRAG_CYLINDER };

    DragMode m_dragMode = DRAG_NONE;
    vtkActor *m_draggedActor = nullptr;
    int m_dragPairIndex = -1;
    double m_dragOffset[3] = { 0, 0, 0};


    // for cylinder drag
    double m_initialMouseWorld[3] = { 0, 0, 0};
    double m_initialSphereAPos[3] = { 0, 0, 0};
    double m_initialSphereBPos[3] = { 0, 0, 0};

    void resetDragState() {
        m_dragMode = DRAG_NONE;
        m_draggedActor = nullptr;
        m_dragPairIndex = -1;
    }


};
vtkStandardNewMacro(SphereInteractorStyle);

 #endif // SPHEREINTERACTORSTYLE_H
