// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/IDevice_Forward.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../Math/Matrix.h"
#include "../Core/Types.h"
#include <memory>

namespace RenderOverlays { namespace DebuggingDisplay { class IInputListener; } }

namespace SceneEngine
{
    class TerrainManager;
    class PlacementsManager;
    class IntersectionTestContext;
    class LightingParserContext;
}

namespace Tools
{
    /// <summary>Basic tools for placing and arranging objects<summary>
    /// To author a world, we need to be able to select, move and place
    /// objects. Normally this kind of work would be done in a complex
    /// gui program. But these tools are intended to help get started with
    /// some basic tools before we have everything we need.
    class PlacementsManipulatorsManager
    {
    public:
        void RenderWidgets(RenderCore::IDevice* device, const Float4x4& viewProjTransform);
        void PlacementsManipulatorsManager::RenderToScene(
            RenderCore::Metal::DeviceContext* context, 
            SceneEngine::LightingParserContext& parserContext);

        std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener> GetInputLister();

        PlacementsManipulatorsManager(
            std::shared_ptr<SceneEngine::PlacementsManager> placementsManager,
            std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
            std::shared_ptr<SceneEngine::IntersectionTestContext> intersectionContext);
        ~PlacementsManipulatorsManager();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}

