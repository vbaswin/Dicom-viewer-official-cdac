#ifndef SPHEREINTERACTORSTYLE_H
#define SPHEREINTERACTORSTYLE_H

#include "vtkRenderWindow.h"
#include <vtkInteractorStyleImage.h>
#include <vtkObjectFactory.h>          // vtkStandardNewMacro
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSphereSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkPropPicker.h>
#include <vtkCoordinate.h>
#include <vtkSmartPointer.h>
#include "mainwindow.h"

#include <vector>


class SphereInteractorStyle : public vtkInteractorStyleImage {
public:
    static SphereInteractorStyle *New();
    vtkTypeMacro(SphereInteractorStyle, vtkInteractorStyleImage);

    void SetAnnotationMode(bool enabled) { m_annotationMode = enabled; }
    bool GetAnnotationMode() const { return m_annotationMode; }


    const std::vector<vtkSmartPointer<vtkActor>> &GetSphereActors() const {
        return m_sphereActors;
    }


    void OnLeftButtonDown() override {
        int *clickPos = this->Interactor->GetEventPosition();
        if (!m_annotationMode) {
            // normal image interactin
            vtkInteractorStyleImage::OnLeftButtonDown();
            return;
        }

        vtkSmartPointer <vtkPropPicker> picker = vtkSmartPointer<vtkPropPicker>::New();

        // pick at 2D display , z depth always 0
        picker->Pick(clickPos[0], clickPos[1], 0, this->CurrentRenderer);

        vtkActor *pickedActor = dynamic_cast<vtkActor *>(picker->GetViewProp());

        if (pickedActor != nullptr && isSphereActor(pickedActor)) {
            m_isDragging = true;
            m_draggedActor = pickedActor;


            // Highlight the picked sphere (visual feedback)
            // Concept: we change the actor's Property — the mapper/source
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
        // emtpy space - create a new sphere
        double worldPos[3];
        displayToWorld(clickPos[0], clickPos[1], worldPos);
        createSphereAt(worldPos);
    }

    // on move - update position if dragging

    void OnMouseMove() override {
        if (m_isDragging && m_draggedActor != nullptr) {
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

            // reques re-render so user sees the sphere move in real-time
            this->Interactor->GetRenderWindow()->Render();
            return; // Don't pass to parent

        }
        vtkInteractorStyleImage::OnMouseMove();
    }
    void OnLeftButtonUp() override {
        if (m_isDragging && m_draggedActor != nullptr ) {
                // Restore original color
                m_draggedActor->GetProperty()->SetColor(1.0, 0.2, 0.2); // red
                m_draggedActor = nullptr;
                m_isDragging = false;

                this->Interactor->GetRenderWindow() ->Render();
                return;
            }
        vtkInteractorStyleImage::OnLeftButtonUp();
    }
private:
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
        m_sphereActors.push_back(actor);
        this->Interactor->GetRenderWindow()->Render();
    }


    // hit test - is this one of our spheres
    bool isSphereActor(vtkActor *actor) const {
        for (const auto &sphereActor: m_sphereActors) {
            if (sphereActor.GetPointer() == actor) {
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

    std::vector<vtkSmartPointer<vtkActor>> m_sphereActors;
    bool m_isDragging = false;
    vtkActor *m_draggedActor = nullptr;
    double m_dragOffset[3] = { 0, 0, 0};

};
vtkStandardNewMacro(SphereInteractorStyle);

 #endif // SPHEREINTERACTORSTYLE_H
