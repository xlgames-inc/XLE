// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../DebuggingDisplay.h"

namespace SceneEngine { class OceanSettings; class OceanLightingSettings; }

namespace Overlays
{
    using namespace RenderOverlays;
    using namespace RenderOverlays::DebuggingDisplay;

    class OceanSettingsDisplay : public IWidget ///////////////////////////////////////////////////////////
    {
    public:
        OceanSettingsDisplay(SceneEngine::OceanSettings& oceanSettings);
        ~OceanSettingsDisplay();
        void    Render(IOverlayContext* context, Layout& layout, Interactables&interactables, InterfaceState& interfaceState);
        bool    ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input);

    private:
        ScrollBar   _scrollers[14];
        SceneEngine::OceanSettings* _oceanSettings;
    };

    class OceanLightingSettingsDisplay : public IWidget ///////////////////////////////////////////////////////////
    {
    public:
        OceanLightingSettingsDisplay(SceneEngine::OceanLightingSettings& oceanSettings);
        ~OceanLightingSettingsDisplay();
        void    Render(IOverlayContext* context, Layout& layout, Interactables&interactables, InterfaceState& interfaceState);
        bool    ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input);

    private:
        ScrollBar   _scrollers[14];
        SceneEngine::OceanLightingSettings* _oceanSettings;
    };

}

