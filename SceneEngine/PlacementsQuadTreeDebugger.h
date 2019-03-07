// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderOverlays/DebuggingDisplay.h"

namespace SceneEngine
{
    class PlacementsManager;
    class PlacementCellSet;

    class PlacementsQuadTreeDebugger : public RenderOverlays::DebuggingDisplay::IWidget
    {
    public:
        typedef RenderOverlays::DebuggingDisplay::Layout Layout;
        typedef RenderOverlays::DebuggingDisplay::Interactables Interactables;
        typedef RenderOverlays::DebuggingDisplay::InterfaceState InterfaceState;

        void    Render( RenderOverlays::IOverlayContext& context, Layout& layout, 
                        Interactables& interactables, InterfaceState& interfaceState);
        bool    ProcessInput(InterfaceState& interfaceState, const PlatformRig::InputContext& inputContext, const PlatformRig::InputSnapshot& input);

        PlacementsQuadTreeDebugger(std::shared_ptr<PlacementsManager> placementsManager, std::shared_ptr<PlacementCellSet> cells);
        ~PlacementsQuadTreeDebugger();
    protected:
        std::shared_ptr<PlacementsManager> _placementsManager;
        std::shared_ptr<PlacementCellSet> _cells;
    };
}

