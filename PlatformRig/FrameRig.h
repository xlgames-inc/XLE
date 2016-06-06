// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/IDevice_Forward.h"
#include "../RenderCore/GPUProfiler.h"
#include <functional>
#include <memory>

namespace RenderOverlays { namespace DebuggingDisplay { class IWidget; class DebugScreensSystem; } }
namespace Utility { class HierarchicalCPUProfiler; }

namespace PlatformRig
{
    class OverlaySystemSet;

    class FrameRig
    {
    public:
        class RenderResult
        {
        public:
            bool _hasPendingResources;
            RenderResult(bool hasPendingResources = false) : _hasPendingResources(hasPendingResources) {}
        };

        class FrameResult
        {
        public:
            float _elapsedTime;
            RenderResult _renderResult;
        };

        typedef std::function<RenderResult(RenderCore::IThreadContext&, const RenderCore::ResourcePtr&)>
            FrameRenderFunction;

        FrameResult ExecuteFrame(
            RenderCore::IThreadContext& context,
            RenderCore::IPresentationChain* presChain,
            RenderCore::IAnnotator* gpuProfiler,
            Utility::HierarchicalCPUProfiler* profiler,
            const FrameRenderFunction& renderFunction);

        void SetFrameLimiter(unsigned maxFPS);
        void SetUpdateAsyncMan(bool updateAsyncMan);

        typedef std::function<void(RenderCore::IThreadContext&)> PostPresentCallback;
        virtual void AddPostPresentCallback(const PostPresentCallback&);

        std::shared_ptr<OverlaySystemSet>& GetMainOverlaySystem();
        std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem>& GetDebugSystem();

        FrameRig(bool isMainFrameRig = true);
        ~FrameRig();

    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

    private:
        FrameRig(const FrameRig& cloneFrom);
        FrameRig& operator=(const FrameRig& cloneFrom);
    };
}

