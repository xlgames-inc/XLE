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

    class PlacementsQuadTreeDebugger : public RenderOverlays::DebuggingDisplay::IWidget
    {
    public:
        typedef RenderOverlays::DebuggingDisplay::Layout Layout;
        typedef RenderOverlays::DebuggingDisplay::Interactables Interactables;
        typedef RenderOverlays::DebuggingDisplay::InterfaceState InterfaceState;
        typedef RenderOverlays::DebuggingDisplay::InputSnapshot InputSnapshot;

        void    Render( RenderOverlays::IOverlayContext* context, Layout& layout, 
                        Interactables& interactables, InterfaceState& interfaceState);
        bool    ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input);

        PlacementsQuadTreeDebugger(std::shared_ptr<PlacementsManager> placementsManager);
        ~PlacementsQuadTreeDebugger();
    protected:
        std::shared_ptr<PlacementsManager> _placementsManager;
    };
}

