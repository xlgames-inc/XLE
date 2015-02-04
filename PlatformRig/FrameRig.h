// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/IDevice.h"
#include "../RenderCore/Metal/Forward.h"
#include <functional>
#include <memory>

namespace RenderOverlays { namespace DebuggingDisplay { class IWidget; class DebugScreensSystem; } }
namespace Utility { class HierarchicalCPUProfiler; }

namespace PlatformRig
{
    class FrameRig
    {
    public:
        class FrameResult
        {
        public:
            float _elapsedTime;
        };

        class RenderResult
        {
        public:
            bool _hasPendingResources;
            RenderResult(bool hasPendingResources) : _hasPendingResources(hasPendingResources) {}
        };

        typedef std::function<RenderResult(RenderCore::Metal::DeviceContext*)>
            FrameRenderFunction;

        FrameResult ExecuteFrame(
            RenderCore::Metal::DeviceContext* context,
            RenderCore::IDevice* device,
            RenderCore::IPresentationChain* presChain,
            RenderCore::Metal::GPUProfiler::Profiler* gpuProfiler,
            const FrameRenderFunction& renderFunction);

        void SetFrameLimiter(unsigned maxFPS);

        FrameRig(
            Utility::HierarchicalCPUProfiler* profiler = nullptr,
            std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem> debugSystem = nullptr);
        ~FrameRig();

    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

    private:
        FrameRig(const FrameRig& cloneFrom);
        FrameRig& operator=(const FrameRig& cloneFrom);
    };
}

