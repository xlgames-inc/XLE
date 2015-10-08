// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4564)   // method 'Copy' of class 'Sce::Atf::Dom::DomNode' defines unsupported default parameter 'originalToCopyMap'

#include "ManipulatorOverlay.h"
#include "XLELayerUtils.h"
#include "NativeManipulators.h"
#include "../../Tools/ToolsRig/VisualisationUtils.h"
#include "../../Tools/GUILayer/NativeEngineDevice.h"
#include "../../Tools/GUILayer/CLIXAutoPtr.h"
#include "../../PlatformRig/OverlaySystem.h"
#include "../../SceneEngine/LightingParserContext.h"
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

namespace XLEBridgeUtils
{
///////////////////////////////////////////////////////////////////////////////////////////////////

    public ref class NativeDesignControl : public DesignViewControl
    {
    public:
        static GUILayer::SimpleRenderingContext^ CreateSimpleRenderingContext(
            GUILayer::SavedRenderResources^ savedRes)
        {
            if (!ManipulatorOverlay::s_currentParsingContext) return nullptr;

            return gcnew GUILayer::SimpleRenderingContext(
                savedRes, ManipulatorOverlay::s_currentParsingContext);
        }

        NativeDesignControl(
            LevelEditorCore::DesignView^ designView, 
            GUILayer::EditorSceneManager^ sceneManager,
            GUILayer::ObjectSet^ selection)
        : DesignViewControl(designView)
        {
            _layerControl = gcnew GUILayer::LayerControl(this);
            _cameraSettings = gcnew GUILayer::VisCameraSettings();
            _renderSettings = gcnew GUILayer::EditorSceneRenderSettings();
            _renderSettings->_selection = selection;
            _sceneManager = sceneManager;
            _mainOverlay = sceneManager->CreateOverlaySystem(_cameraSettings, _renderSettings);
            _layerControl->AddSystem(_mainOverlay);
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

        ~NativeDesignControl() 
        { 
            delete _layerControl; _layerControl = nullptr; 
            delete _cameraSettings; _cameraSettings = nullptr; 
            delete _renderSettings; _renderSettings = nullptr; 
            delete _manipulatorOverlay; _manipulatorOverlay = nullptr; 
            delete _mainOverlay; _mainOverlay = nullptr;
        }

        void Render() override
        {
                //  "_cameraSettings" should match the camera set in 
                //  the view control
            auto camera = Camera;
            auto& dstCam = *_cameraSettings->GetUnderlyingRaw();
            dstCam._position = AsFloat3(camera->WorldEye);
            dstCam._focus = AsFloat3(camera->WorldLookAtPoint);
            dstCam._nearClip = camera->NearZ;
            dstCam._farClip = camera->FarZ;

            if (camera->ProjectionType == Sce::Atf::Rendering::ProjectionType::Orthographic) {
                dstCam._projection = ToolsRig::VisCameraSettings::Projection::Orthogonal;
                dstCam._left = camera->Frustum->Left;
                dstCam._right = camera->Frustum->Right;
                dstCam._top = camera->Frustum->Top;
                dstCam._bottom = camera->Frustum->Bottom;
            } else {
                dstCam._projection = ToolsRig::VisCameraSettings::Projection::Perspective;
                dstCam._verticalFieldOfView = camera->YFov * 180.f / gPI;
            }
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
        GUILayer::EditorSceneRenderSettings^ _renderSettings;
        GUILayer::IOverlaySystem^ _mainOverlay;

    private:
        ManipulatorOverlay^ _manipulatorOverlay;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    public interface class IManipulatorExtra
    {
    public:
        virtual bool ClearBeforeDraw() = 0;
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
            if (_designView->Manipulator != nullptr) {

                bool clearBeforeDraw = true;
                IManipulatorExtra^ extra = dynamic_cast<IManipulatorExtra^>(_designView->Manipulator);
                if (extra)
                    clearBeforeDraw = extra->ClearBeforeDraw();

                if (clearBeforeDraw) {
                        // disable depth write and depth read
                    auto context = gcnew GUILayer::SimpleRenderingContext(nullptr, ManipulatorOverlay::s_currentParsingContext);
                    GUILayer::RenderingUtil::ClearDepthBuffer(context);
                    context->InitState(true, true);
                    delete context;
                }

                _designView->Manipulator->Render(_viewControl);
            }
        }
        catch (...)
        {
            s_currentParsingContext = nullptr;
            throw;
        }

        s_currentParsingContext = nullptr;
    }

    void ManipulatorOverlay::RenderWidgets(
        RenderCore::IThreadContext* device, 
        const RenderCore::Techniques::ProjectionDesc& projectionDesc) {}
    void ManipulatorOverlay::SetActivationState(bool) {}

    ManipulatorOverlay::ManipulatorOverlay(
        LevelEditorCore::DesignView^ designView,
        LevelEditorCore::ViewControl^ viewControl)
    : _designView(designView), _viewControl(viewControl) {}
    ManipulatorOverlay::~ManipulatorOverlay() {}
    ManipulatorOverlay::!ManipulatorOverlay() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    GUILayer::EditorSceneManager^ Utils::GetSceneManager(LevelEditorCore::ViewControl^ vc)
    {
        auto native = dynamic_cast<NativeDesignControl^>(vc);
        if (!native) return nullptr;
        return native->SceneManager;
    }

    GUILayer::TechniqueContextWrapper^ Utils::GetTechniqueContext(LevelEditorCore::ViewControl^ vc)
    {
        auto native = dynamic_cast<NativeDesignControl^>(vc);
        if (!native) return nullptr;
        return native->TechniqueContext;
    }
}

