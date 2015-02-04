// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CPUProfileDisplay.h"
#include "../../Utility/Profiling/CPUProfiler.h"
#include "../../Utility/StringFormat.h"
#include <stack>
#include <iomanip>

namespace PlatformRig { namespace Overlays
{
    static float AsMilliseconds(uint64 profilerTime)
    {
        static float freqMult = 1000.f / GetPerformanceCounterFrequency();
        return float(profilerTime) * freqMult;
    }

    void CPUProfileDisplay::Render(
        IOverlayContext* context, Layout& layout, 
        Interactables&interactables, InterfaceState& interfaceState)
    {
        using namespace RenderOverlays::DebuggingDisplay;

        auto resolvedEvents = _profiler->CalculateResolvedEvents();
        Layout hierarchyView(layout.GetMaximumSize());
        const unsigned lineHeight = 20;

            //  The resolved events are arranged as a tree. We just want
            //  to traverse in depth-first order, and display as a tree

            // (this stack is limited to depth HierarchicalCPUProfiler::s_maxStackDepth)
        static std::stack<std::pair<unsigned, unsigned>> items;
        auto rootItem = 0;
        while (rootItem != HierarchicalCPUProfiler::ResolvedEvent::s_id_Invalid) {
            items.push(std::make_pair(rootItem,0));
            rootItem = resolvedEvents[rootItem]._sibling;
        }

        while (!items.empty()) {
            unsigned treeDepth = items.top().second;
            const auto& evnt = resolvedEvents[items.top().first];
            items.pop();

            auto rect = layout.AllocateFullWidth(lineHeight);
            DrawText(
                context, Rect(rect._topLeft + UInt2(treeDepth * 16, 0), rect._bottomRight), 
                0.f, 1.f, nullptr, RenderOverlays::ColorB(0xffffffff), 
                StringMeld<256>() << std::setprecision(5) << evnt._label << " (" << evnt._eventCount << ") " << AsMilliseconds(evnt._inclusiveTime));

                // push all children onto the stack
            auto child = evnt._firstChild;
            while (child != HierarchicalCPUProfiler::ResolvedEvent::s_id_Invalid) {
                items.push(std::make_pair(child, treeDepth+1));
                child = resolvedEvents[child]._sibling;
            }
        }
    }

    bool CPUProfileDisplay::ProcessInput(
        InterfaceState& interfaceState, const InputSnapshot& input)
    {
        return false;
    }
        
    CPUProfileDisplay::CPUProfileDisplay(HierarchicalCPUProfiler* profiler)
        : _profiler(profiler)
    {}

    CPUProfileDisplay::~CPUProfileDisplay() {}
}}

