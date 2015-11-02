// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderOverlays/DebuggingDisplay.h"
#include <memory>

namespace RenderOverlays { class IOverlayContext; namespace DebuggingDisplay { class InputSnapshot; class IInputListener; class InterfaceState; struct Layout; class Interactables; class DebugScreensSystem; }; class Font; }
namespace SceneEngine { class LightingParserContext; class TerrainManager; class IntersectionTestContext; class IntersectionTestScene; }

namespace ToolsRig
{
    class IManipulator;

    class ManipulatorsInterface : public std::enable_shared_from_this<ManipulatorsInterface>
    {
    public:
        void    Render( RenderCore::IThreadContext& context, 
                        SceneEngine::LightingParserContext& parserContext);
        void    Update();

        void SelectManipulator(signed relativeIndex);
        IManipulator* GetActiveManipulator() const { return _manipulators[_activeManipulatorIndex].get(); }

        std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener>   CreateInputListener();

        ManipulatorsInterface(
            std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
            std::shared_ptr<SceneEngine::IntersectionTestContext> intersectionTestContext);
        ~ManipulatorsInterface();
    private:
        std::vector<std::unique_ptr<IManipulator>> _manipulators;
        unsigned _activeManipulatorIndex;

        std::shared_ptr<SceneEngine::TerrainManager>            _terrainManager;
        std::shared_ptr<SceneEngine::IntersectionTestContext>   _intersectionTestContext;
        std::shared_ptr<SceneEngine::IntersectionTestScene>     _intersectionTestScene;

        class InputListener;
    };

    class ManipulatorsDisplay : public RenderOverlays::DebuggingDisplay::IWidget
    {
    public:
        void    Render( RenderOverlays::IOverlayContext* context, RenderOverlays::DebuggingDisplay::Layout& layout, 
                        RenderOverlays::DebuggingDisplay::Interactables&interactables, 
                        RenderOverlays::DebuggingDisplay::InterfaceState& interfaceState);
        bool    ProcessInput(RenderOverlays::DebuggingDisplay::InterfaceState& interfaceState, const RenderOverlays::DebuggingDisplay::InputSnapshot& input);

        ManipulatorsDisplay(std::shared_ptr<ManipulatorsInterface> interf);
        ~ManipulatorsDisplay();

    private:
        std::shared_ptr<ManipulatorsInterface> _manipulatorsInterface;
    };
}


