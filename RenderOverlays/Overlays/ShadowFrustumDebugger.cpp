// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShadowFrustumDebugger.h"
#include "../IOverlayContext.h"
#include "../../SceneEngine/CommonResources.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../Math/Transformations.h"

namespace Overlays
{
    using namespace SceneEngine;
    using namespace RenderOverlays;

    void ShadowFrustumDebugger::Render( 
        IOverlayContext* context, Layout& layout, 
        Interactables& interactables, InterfaceState& interfaceState)
    {
        assert(_scene.get());

        if (!_scene->GetShadowProjectionCount()) {
            return;
        }

            //  Get the first shadow projection from the scene, and draw an
            //  outline of all sub-projections with in.
            //  We could also add a control to select different projections
            //  when there are more than one...
        context->GetDeviceContext()->Bind(CommonResources()._dssDisable);

        ColorB cols[]= {
            ColorB(196, 230, 230),
            ColorB(255, 128, 128),
            ColorB(128, 255, 128),
            ColorB(128, 128, 255),
            ColorB(255, 255, 128)
        };

        auto projectionDesc = _scene->GetShadowProjectionDesc(0, context->GetProjectionDesc());
        const auto& projections = projectionDesc._projections;
        for (unsigned c=0; c<projections._count; ++c) {
            DebuggingDisplay::DrawFrustum(
                context, Combine(projections._fullProj[c]._viewMatrix, projections._fullProj[c]._projectionMatrix),
                cols[std::min(dimof(cols), c)], 0x1);
        }

        for (unsigned c=0; c<projections._count; ++c) {
            DebuggingDisplay::DrawFrustum(
                context, Combine(projections._fullProj[c]._viewMatrix, projections._fullProj[c]._projectionMatrix),
                cols[std::min(dimof(cols), c)], 0x2);
        }
    }

    bool ShadowFrustumDebugger::ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input)
    {
        return false;
    }

    ShadowFrustumDebugger::ShadowFrustumDebugger(std::shared_ptr<SceneEngine::ISceneParser> scene)
    : _scene(std::move(scene))
    {}

    ShadowFrustumDebugger::~ShadowFrustumDebugger()
    {
    }

}

