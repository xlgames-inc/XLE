// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../RenderCore/Metal/GPUProfiler.h"

namespace Utility { class HierarchicalCPUProfiler; }

namespace PlatformRig { namespace Overlays
{
    class CPUProfileDisplay : public RenderOverlays::DebuggingDisplay::IWidget ///////////////////////////////////////////////////////////
    {
    public:
        typedef RenderOverlays::IOverlayContext IOverlayContext;
        typedef RenderOverlays::DebuggingDisplay::Layout Layout;
        typedef RenderOverlays::DebuggingDisplay::Interactables Interactables;
        typedef RenderOverlays::DebuggingDisplay::InterfaceState InterfaceState;
        typedef RenderOverlays::DebuggingDisplay::InputSnapshot InputSnapshot;

        void    Render(IOverlayContext* context, Layout& layout, Interactables&interactables, InterfaceState& interfaceState);
        bool    ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input);
        
        CPUProfileDisplay(HierarchicalCPUProfiler* profiler);
        ~CPUProfileDisplay();
    private:
        HierarchicalCPUProfiler* _profiler;
        std::vector<uint64> _toggledItems;
    };
}}

