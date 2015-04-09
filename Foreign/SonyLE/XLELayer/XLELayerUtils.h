// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../Tools/ToolsRig/VisualisationUtils.h"
#include "../../Math/Vector.h"
#include "../../Math/Transformations.h"

using namespace System;
using namespace Sce::Atf;
using namespace Sce::Atf::Applications;

namespace XLELayer
{
    static Float3 AsFloat3(Sce::Atf::VectorMath::Vec3F^ input) { return Float3(input->X, input->Y, input->Z); }

    public ref class XLELayerUtils
    {
    public:
        static RenderCore::Techniques::CameraDesc AsCameraDesc(Sce::Atf::Rendering::Camera^ camera)
        {
            ToolsRig::VisCameraSettings visCam;
            visCam._position = AsFloat3(camera->WorldEye);
            visCam._focus = AsFloat3(camera->WorldLookAtPoint);
            visCam._verticalFieldOfView = camera->YFov * 180.f / gPI;
            visCam._nearClip = camera->NearZ;
            visCam._farClip = camera->FarZ;
            return RenderCore::Techniques::CameraDesc(ToolsRig::AsCameraDesc(visCam));
        }

        static GUILayer::IntersectionTestContextWrapper^
            CreateIntersectionTestContext(
                GUILayer::EngineDevice^ engineDevice,
                GUILayer::TechniqueContextWrapper^ techniqueContext,
                Sce::Atf::Rendering::Camera^ camera,
                unsigned viewportWidth, unsigned viewportHeight)
        {
            return GUILayer::EditorInterfaceUtils::CreateIntersectionTestContext(
                engineDevice, techniqueContext, 
                AsCameraDesc(camera), viewportWidth, viewportHeight);
        }

        static Sce::Atf::VectorMath::Matrix4F^ MakeFrustumMatrix(
            Sce::Atf::Rendering::Camera^ camera,
            System::Drawing::RectangleF rectangle,
            System::Drawing::Size viewportSize)
        {
                //  Given a camera and rectangle, calculate a
                //  frustum matrix that will represents that area.
            auto camDesc = AsCameraDesc(camera);
            auto proj = RenderCore::Techniques::PerspectiveProjection(
                camDesc, viewportSize.Width / float(viewportSize.Height));
            auto worldToProj = Combine(
                InvertOrthonormalTransform(camDesc._cameraToWorld), proj);

            System::Drawing::RectangleF fRect(
                rectangle.Left / float(viewportSize.Width), rectangle.Top / float(viewportSize.Height),
                rectangle.Width / float(viewportSize.Width), rectangle.Height / float(viewportSize.Height));

                // skew XY in the projection matrix to suit the rectangle
            float sx = 1.f / (fRect.Width);
            float sy = 1.f / (fRect.Height);
            float tx = -(2.f * .5f * (fRect.Left + fRect.Right) - 1.f) * sx;
            float ty = -(-2.f * .5f * (fRect.Top + fRect.Bottom) + 1.f) * sy;

            Float4x4 rectangleAdj = MakeFloat4x4(
                 sx, 0.f, 0.f,  tx,
                0.f,  sy, 0.f,  ty,
                0.f, 0.f, 1.f, 0.f,
                0.f, 0.f, 0.f, 1.f);

            worldToProj = Combine(worldToProj, rectangleAdj);

                // note -- forcing a transpose here!
            return gcnew Sce::Atf::VectorMath::Matrix4F(
                worldToProj(0,0), worldToProj(1,0), worldToProj(2,0), worldToProj(3,0),
                worldToProj(0,1), worldToProj(1,1), worldToProj(2,1), worldToProj(3,1),
                worldToProj(0,2), worldToProj(1,2), worldToProj(2,2), worldToProj(3,2),
                worldToProj(0,3), worldToProj(1,3), worldToProj(2,3), worldToProj(3,3));
        }
    };

    private ref class DomChangeInspector
    {
    public:
        delegate void OnChangedDelegate(System::Object^);
        event OnChangedDelegate^ OnActiveContextChanged;
        event OnChangedDelegate^ OnDOMObjectChanged;

        DomChangeInspector(IContextRegistry^ contextRegistry) 
            : m_contextRegistry(contextRegistry)
        {
            m_observableContext = nullptr;
            contextRegistry->ActiveContextChanged += 
                gcnew EventHandler(this, &DomChangeInspector::ContextRegistry_ActiveContextChanged);
        }

    private:
        void ContextRegistry_ActiveContextChanged(System::Object^ sender, EventArgs^ e)
        {
            using namespace LevelEditorCore;
            IGameContext^ game = m_contextRegistry->GetActiveContext<IGameContext^>();
            auto observableContext = Sce::Atf::Adaptation::Adapters::As<IObservableContext^>(game);
            if (m_observableContext == observableContext) return;
            if (m_observableContext != nullptr) {
                m_observableContext->ItemInserted -= gcnew EventHandler<ItemInsertedEventArgs<System::Object^>^>(this, &DomChangeInspector::m_observableContext_ItemInserted);
                m_observableContext->ItemRemoved -= gcnew EventHandler<ItemRemovedEventArgs<System::Object^>^>(this, &DomChangeInspector::m_observableContext_ItemRemoved);
                m_observableContext->ItemChanged -= gcnew EventHandler<Sce::Atf::ItemChangedEventArgs<System::Object^>^>(this, &DomChangeInspector::m_observableContext_ItemChanged);
                m_observableContext->Reloaded -= gcnew EventHandler(this, &DomChangeInspector::m_observableContext_Reloaded);
            }
            m_observableContext = observableContext;

            if (m_observableContext != nullptr) {
                m_observableContext->ItemInserted += gcnew EventHandler<ItemInsertedEventArgs<System::Object^>^>(this, &DomChangeInspector::m_observableContext_ItemInserted);
                m_observableContext->ItemRemoved += gcnew EventHandler<ItemRemovedEventArgs<System::Object^>^>(this, &DomChangeInspector::m_observableContext_ItemRemoved);
                m_observableContext->ItemChanged += gcnew EventHandler<Sce::Atf::ItemChangedEventArgs<System::Object^>^>(this, &DomChangeInspector::m_observableContext_ItemChanged);
                m_observableContext->Reloaded += gcnew EventHandler(this, &DomChangeInspector::m_observableContext_Reloaded);
            }
            OnActiveContextChanged(sender);
        }

        void m_observableContext_ItemChanged(System::Object^ sender, Sce::Atf::ItemChangedEventArgs<System::Object^>^ e) { OnDOMObjectChanged(e->Item); }
        void m_observableContext_ItemRemoved(System::Object^ sender, ItemRemovedEventArgs<System::Object^>^ e) { OnDOMObjectChanged(e->Item); }
        void m_observableContext_ItemInserted(System::Object^ sender, ItemInsertedEventArgs<System::Object^>^ e) { OnDOMObjectChanged(e->Item); }
        void m_observableContext_Reloaded(System::Object^ sender, EventArgs^ e) { OnDOMObjectChanged(nullptr); }
        
        IObservableContext^ m_observableContext;
        IContextRegistry^ m_contextRegistry;
    };
}

