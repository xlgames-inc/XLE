// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ManipulatorOverlay.h"
#include "../../PlatformRig/ModelVisualisation.h"
#include "../../PlatformRig/OverlaySystem.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../Tools/GUILayer/NativeEngineDevice.h"
#include "../../RenderCore/IDevice.h"
#include <memory>

using namespace System;
using namespace System::Windows::Forms;
using namespace System::Drawing;
using namespace System::Collections::Generic;
using namespace LevelEditorCore::VectorMath;
using namespace Sce::Atf;
using namespace Sce::Atf::Adaptation;
using namespace Sce::Atf::Applications;
using namespace Sce::Atf::Dom;
using namespace Sce::Atf::VectorMath;
using namespace LevelEditorCore;

#pragma warning(disable:4564)   // method 'Copy' of class 'Sce::Atf::Dom::DomNode' defines unsupported default parameter 'originalToCopyMap'

namespace XLELayer
{
    static Float3 AsFloat3(Vec3F^ input) { return Float3(input->X, input->Y, input->Z); }

///////////////////////////////////////////////////////////////////////////////////////////////////

    public ref class NativeDesignControl : public DesignViewControl
    {
    public:
        static GUILayer::SimpleRenderingContext^ CreateSimpleRenderingContext(GUILayer::SavedRenderResources^ savedRes)
        {
            if (!ManipulatorOverlay::s_currentParsingContext) return nullptr;

            return gcnew GUILayer::SimpleRenderingContext(
                savedRes, 
                *GUILayer::EngineDevice::GetInstance()->GetNative().GetRenderDevice()->GetImmediateContext(),
                ManipulatorOverlay::s_currentParsingContext);
        }

        NativeDesignControl(
            LevelEditorCore::DesignView^ designView, 
            GUILayer::EditorSceneManager^ sceneManager)
        : DesignViewControl(designView)
        {
            _layerControl = gcnew GUILayer::LayerControl(this);
            _cameraSettings = gcnew GUILayer::VisCameraSettings();
            _sceneManager = sceneManager;
            _layerControl->AddSystem(sceneManager->CreateOverlaySystem(_cameraSettings));
            _manipulatorOverlay = gcnew ManipulatorOverlay(designView, this);
            _layerControl->AddSystem(_manipulatorOverlay);

                //  use the camera "AxisSystem" to convert from the native SonyWWS
                //  camera coordinates into the coordinates we need for XLE.
                //      SonyWWS native has +Y as up
                //      But XLE uses +Z up
                //  note -- handiness untested. XLE defaults to right handed coordinates in
                //          view space.
                //
                //  When using AxisSystem like this, we must use the "World..." properties 
                //  in the Camera object. eg, "WorldEye" gives us the native XLE coordinates,
                //  "Eye" gives us the native SonyWWS coordinates.
            Camera->AxisSystem = gcnew Matrix4F(
                1.f, 0.f, 0.f, 0.f, 
                0.f, 0.f, -1.f, 0.f, 
                0.f, 1.f, 0.f, 0.f, 
                0.f, 0.f, 0.f, 1.f);
        }

        ~NativeDesignControl() { delete _layerControl; _layerControl = nullptr; delete _cameraSettings; _cameraSettings = nullptr; }

        void Render() override 
        {
                //  "_cameraSettings" should match the camera set in 
                //  the view control
            auto camera = Camera;
            _cameraSettings->GetUnderlyingRaw()->_position = AsFloat3(camera->WorldEye);
            _cameraSettings->GetUnderlyingRaw()->_focus = AsFloat3(camera->WorldLookAtPoint);
            _cameraSettings->GetUnderlyingRaw()->_verticalFieldOfView = camera->YFov * 180.f / gPI;
            _cameraSettings->GetUnderlyingRaw()->_nearClip = camera->NearZ;
            _cameraSettings->GetUnderlyingRaw()->_farClip = camera->FarZ;
            _layerControl->Render();

                // testing for agreement between SonyWWS camera code and XLE camera code
            // float viewportAspect = Width / float(Height);
            // auto camDesc = PlatformRig::AsCameraDesc(*_cameraSettings->GetUnderlyingRaw());
            // auto proj = RenderCore::Techniques::PerspectiveProjection(
            //     camDesc, viewportAspect);
            // auto viewMat = InvertOrthonormalTransform(camDesc._cameraToWorld);
            // auto compViewMat = Camera->ViewMatrix;
            // auto compProjMat = Camera->ProjectionMatrix;
            // (void)compViewMat; (void)compProjMat; (void)proj; (void)viewMat;
        }

        void AddRenderCallback(RenderCallback^ callback)
        {
            _manipulatorOverlay->OnRender += callback;
        }

        property GUILayer::EditorSceneManager^ SceneManager {
            GUILayer::EditorSceneManager^ get() { return _sceneManager; }
        }
        property GUILayer::TechniqueContextWrapper^ TechniqueContext {
            GUILayer::TechniqueContextWrapper^ get() { return _layerControl->GetTechniqueContext(); }
        }

    protected:
        void OnResize(System::EventArgs^ e) override
        {
            _layerControl->OnResize(e);
            auto sz = ClientSize;
            if (sz.Width > 0 && sz.Height > 0)
            {
                Camera->Aspect = (float)sz.Width / (float)sz.Height;
            }
            __super::OnResize(e);
        }

        GUILayer::LayerControl^ _layerControl;
        GUILayer::VisCameraSettings^ _cameraSettings;
        GUILayer::EditorSceneManager^ _sceneManager;

    private:
        ManipulatorOverlay^ _manipulatorOverlay;
    };


    void ManipulatorOverlay::RenderToScene(
        RenderCore::IThreadContext* device, 
        SceneEngine::LightingParserContext& parserContext)
    {
        using namespace LevelEditorCore;
        s_currentParsingContext = &parserContext;
            
        try
        {
            OnRender(_designView, _viewControl->Camera);
            if (_designView->Manipulator != nullptr)
                _designView->Manipulator->Render(_viewControl);
        }
        catch (...)
        {
            s_currentParsingContext = nullptr;
            throw;
        }

        s_currentParsingContext = nullptr;
    }
}

