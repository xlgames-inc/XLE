// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/IDevice_Forward.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../Math/Matrix.h"
#include "../../Core/Types.h"
#include <memory>

namespace RenderOverlays { namespace DebuggingDisplay { class IInputListener; } }
namespace RenderCore { namespace Techniques { class ProjectionDesc; } }

namespace SceneEngine
{
    class TerrainManager;
    class PlacementsManager;
    class IntersectionTestContext;
    class LightingParserContext;
    class PlacementsEditor;
    typedef std::pair<uint64, uint64> PlacementGUID;
}

namespace ToolsRig
{
    /// <summary>Basic tools for placing and arranging objects<summary>
    /// To author a world, we need to be able to select, move and place
    /// objects. Normally this kind of work would be done in a complex
    /// gui program. But these tools are intended to help get started with
    /// some basic tools before we have everything we need.
    class PlacementsManipulatorsManager
    {
    public:
        void RenderWidgets(
            RenderCore::IThreadContext* device, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc);
        void RenderToScene(
            RenderCore::IThreadContext* context, 
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

    class IManipulator;

    class IPlacementManipulatorSettings
    {
    public:
        virtual std::string GetSelectedModel() const = 0;
        virtual void EnableSelectedModelDisplay(bool newState) = 0;
        virtual void SelectModel(const char newModelName[]) = 0;

        struct Mode { enum Enum { Select, PlaceSingle }; };
        virtual void SwitchToMode(Mode::Enum newMode) = 0;

        virtual ~IPlacementManipulatorSettings();
    };

    std::vector<std::unique_ptr<IManipulator>> CreatePlacementManipulators(
        IPlacementManipulatorSettings* context,
        std::shared_ptr<SceneEngine::PlacementsEditor> editor);

    void RenderHighlight(
        RenderCore::Metal::DeviceContext* context,
        SceneEngine::LightingParserContext& parserContext,
        SceneEngine::PlacementsEditor* editor,
        const SceneEngine::PlacementGUID* filterBegin,
        const SceneEngine::PlacementGUID* filterEnd);
}

