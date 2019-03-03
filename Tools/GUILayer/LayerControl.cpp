// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4793) //  : function compiled as native :

#include "LayerControl.h"
#include "IWindowRig.h"
#include "IOverlaySystem.h"
#include "UITypesBinding.h"
#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "GUILayerUtil.h"
#include "ExportedNativeTypes.h"
#include "../ToolsRig/ModelVisualisation.h"
#include "../ToolsRig/IManipulator.h"
#include "../ToolsRig/BasicManipulators.h"
#include "../ToolsRig/VisualisationUtils.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/OverlaySystem.h"

#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../RenderOverlays/Font.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/LightingParserStandardPlugin.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../SceneEngine/VegetationSpawn.h"
#include "../../RenderCore/IThreadContext.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/Techniques/RenderPassUtils.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../FixedFunctionModel/ModelCache.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringFormat.h"
#include <stack>
#include <iomanip>

using namespace System;

namespace GUILayer 
{
    bool LayerControl::Render(RenderCore::IThreadContext& threadContext, IWindowRig& windowRig)
    {
            // Rare cases can recursively start rendering
            // (for example, if we attempt to call a Windows GUI function while in the middle of
            // rendering)
            // Re-entering rendering recursively can cause some bad problems, however
            //  -- so we need to prevent it.
        if (_pimpl->_activePaint)
            return false;

            // Check for cases where a paint operation can be begun on one window
            // while another window is in the middle of rendering.
        static bool activePaintCheck2 = false;
        if (activePaintCheck2) return false;

        _pimpl->_activePaint = true;
        activePaintCheck2 = true;
        
        bool result = true;
        TRY
        {
            auto& frameRig = windowRig.GetFrameRig();
			RenderCore::Techniques::ParsingContext parserContext(*_pimpl->_globalTechniqueContext, _pimpl->_namedResources.get(), _pimpl->_frameBufferPool.get());
            auto frResult = frameRig.ExecuteFrame(
                threadContext, windowRig.GetPresentationChain().get(), 
                parserContext, nullptr);

            // return false if when we have pending resources (encourage another redraw)
            result =  !frResult._renderResult._hasPendingResources;
        } CATCH (...) {
        } CATCH_END
        activePaintCheck2 = false;
        _pimpl->_activePaint = false;

        return result;
    }

	void LayerControl::OnResize()
    {
		// We must reset the framebuffer in order to dump references to the presentation chain on DX (because it's going to be resized along with the window)
		_pimpl->_frameBufferPool->Reset();
	}

///////////////////////////////////////////////////////////////////////////////////////////////////
    
    void LayerControl::SetUpdateAsyncMan(bool updateAsyncMan)
    {
        GetWindowRig().GetFrameRig().SetUpdateAsyncMan(updateAsyncMan);
    }

    namespace Internal
    {
        class OverlaySystemAdapter : public PlatformRig::IOverlaySystem
        {
        public:
            typedef RenderOverlays::DebuggingDisplay::IInputListener IInputListener;

            std::shared_ptr<IInputListener> GetInputListener()
            {
                // return _managedOverlay->GetInputListener();
                return nullptr;
            }

            void Render(
                RenderCore::IThreadContext& threadContext,
				const RenderCore::IResourcePtr& renderTarget,
                RenderCore::Techniques::ParsingContext& parserContext)
            {
				auto rpi = RenderCore::Techniques::RenderPassToPresentationTargetWithDepthStencil(threadContext, renderTarget, parserContext);
				_managedOverlay->Render(threadContext, RenderTargetWrapper{renderTarget}, parserContext);
            }

            void SetActivationState(bool newState)
            {
                _managedOverlay->SetActivationState(newState);
            }

            OverlaySystemAdapter(::GUILayer::IOverlaySystem^ managedOverlay) : _managedOverlay(managedOverlay) {}
            ~OverlaySystemAdapter() {}
        protected:
            msclr::gcroot<::GUILayer::IOverlaySystem^> _managedOverlay;
        };
    }

    void LayerControl::AddSystem(IOverlaySystem^ overlay)
    {
        auto& overlaySet = *GetWindowRig().GetFrameRig().GetMainOverlaySystem();
        overlaySet.AddSystem(std::shared_ptr<Internal::OverlaySystemAdapter>(
            new Internal::OverlaySystemAdapter(overlay)));
    }

    void LayerControl::AddDefaultCameraHandler(VisCameraSettings^ settings)
    {
            // create an input listener that feeds into a stack of manipulators
        auto manipulators = std::make_shared<ToolsRig::ManipulatorStack>();
        manipulators->Register(
            ToolsRig::ManipulatorStack::CameraManipulator,
            ToolsRig::CreateCameraManipulator(settings->GetUnderlying()));

        auto& overlaySet = *GetWindowRig().GetFrameRig().GetMainOverlaySystem();
        overlaySet.AddSystem(ToolsRig::MakeLayerForInput(manipulators));
    }

    TechniqueContextWrapper^ LayerControl::GetTechniqueContext()
    {
        return _techContextWrapper;
    }

	void LayerControl::OnEngineShutdown()
	{
		_pimpl.reset();
		delete _techContextWrapper;
		_techContextWrapper = nullptr;
		EngineControl::OnEngineShutdown();
	}

    LayerControl::LayerControl(System::Windows::Forms::Control^ control)
        : EngineControl(control)
    {
        _pimpl.reset(new LayerControlPimpl());
        _pimpl->_globalTechniqueContext = std::make_shared<RenderCore::Techniques::TechniqueContext>();
        _pimpl->_namedResources = std::make_shared<RenderCore::Techniques::AttachmentPool>();
		_pimpl->_frameBufferPool = std::make_shared<RenderCore::Techniques::FrameBufferPool>();
        _techContextWrapper = gcnew TechniqueContextWrapper(_pimpl->_globalTechniqueContext);
        _pimpl->_activePaint = false;
    }

    LayerControl::~LayerControl() 
    {
		_pimpl.reset();
        delete _techContextWrapper;
		_techContextWrapper = nullptr;
    }

    LayerControl::!LayerControl()
    {
		if (_pimpl.get()) {
			System::Diagnostics::Debug::Assert(false, "Non deterministic delete of LayerControl");
		}
    }
}

