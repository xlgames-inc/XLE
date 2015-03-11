// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4793) //  : function compiled as native :

#include "LayerControl.h"
#include "EngineControlInternal.h"
#include "IWindowRig.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../PlatformRig/PlatformRigUtil.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/OverlaySystem.h"
#include "../../PlatformRig/ModelVisualisation.h"

#include "../../RenderOverlays/Font.h"
#include "../../SceneEngine/SceneEngineUtility.h"
#include "../../SceneEngine/LightingParserStandardPlugin.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../Utility/PtrUtils.h"

namespace GUILayer 
{
    class LayerControlPimpl 
    {
    public:
        std::shared_ptr<SceneEngine::LightingParserStandardPlugin> _stdPlugin;
        std::shared_ptr<PlatformRig::GlobalTechniqueContext> _globalTechniqueContext;
    };

    static PlatformRig::FrameRig::RenderResult RenderFrame(
        RenderCore::IThreadContext& context,
        LayerControlPimpl& pimpl,
        PlatformRig::IOverlaySystem* overlaySys)
    {
        using namespace SceneEngine;

        LightingParserContext lightingParserContext(*pimpl._globalTechniqueContext);
        lightingParserContext._plugins.push_back(pimpl._stdPlugin);

        if (overlaySys) {
            overlaySys->RenderToScene(&context, lightingParserContext);
        }

        ///////////////////////////////////////////////////////////////////////
        bool hasPendingResources = !lightingParserContext._pendingResources.empty();
        if (hasPendingResources) {
            auto metalContext = RenderCore::Metal::DeviceContext::Get(context);
            auto defaultFont0 = RenderOverlays::GetX2Font("Raleway", 16);
            DrawPendingResources(metalContext.get(), lightingParserContext, defaultFont0.get());
        }
        ///////////////////////////////////////////////////////////////////////

        if (overlaySys) {
            overlaySys->RenderWidgets(&context, lightingParserContext.GetProjectionDesc());
        }

        return PlatformRig::FrameRig::RenderResult(hasPendingResources);
    }

    void LayerControl::Render(RenderCore::IThreadContext& threadContext, IWindowRig& windowRig)
    {
        auto& frameRig = windowRig.GetFrameRig();
        frameRig.ExecuteFrame(
            threadContext, windowRig.GetPresentationChain().get(), 
            nullptr, nullptr,
            std::bind(
                RenderFrame, std::placeholders::_1,
                std::ref(*_pimpl), frameRig.GetMainOverlaySystem().get()));
    }

    static std::shared_ptr<PlatformRig::ModelVisCache> s_visCache;
    
    void LayerControl::SetupDefaultVis()
    {
        if (!s_visCache) {
            s_visCache = std::make_shared<PlatformRig::ModelVisCache>(
                std::shared_ptr<RenderCore::Assets::IModelFormat>());
        }

        auto visLayer = std::make_unique<PlatformRig::ModelVisLayer>(s_visCache);
        auto& overlaySet = *GetWindowRig().GetFrameRig().GetMainOverlaySystem();
        overlaySet.AddSystem(std::move(visLayer));
    }

    LayerControl::LayerControl()
    {
        _pimpl.reset(new LayerControlPimpl());
        _pimpl->_stdPlugin = std::make_shared<SceneEngine::LightingParserStandardPlugin>();
        _pimpl->_globalTechniqueContext = std::make_shared<PlatformRig::GlobalTechniqueContext>();
    }

	LayerControl::~LayerControl()
    {}
}

