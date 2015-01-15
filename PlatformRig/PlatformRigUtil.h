// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "OverlappedWindow.h"
#include "../SceneEngine/Techniques.h"
#include "../RenderCore/IDevice_Forward.h"

namespace RenderOverlays { namespace DebuggingDisplay { class DebugScreensSystem; }}
namespace RenderCore { class CameraDesc; }
namespace SceneEngine { class ShadowFrustumDesc; class LightDesc; }

namespace PlatformRig
{
///////////////////////////////////////////////////////////////////////////////////////////////////

    class GlobalTechniqueContext : public SceneEngine::TechniqueContext
    {
    public:
        void SetInteger(const char name[], uint32 value);

        GlobalTechniqueContext();
        ~GlobalTechniqueContext();
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    /// <summary>Resizes a presentation chain to match a window</summary>
    /// A handler to resize the presentation chain whenever the window
    /// changes size (keeping 1:1 ratio)
    class ResizePresentationChain : public PlatformRig::IWindowHandler
    {
    public:
        void    OnResize(unsigned newWidth, unsigned newHeight);

        ResizePresentationChain(
            std::shared_ptr<RenderCore::IPresentationChain> presentationChain);
    protected:
        std::shared_ptr<RenderCore::IPresentationChain> _presentationChain;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    SceneEngine::ShadowFrustumDesc CalculateDefaultShadowFrustums(
        const SceneEngine::LightDesc& lightDesc,
        const RenderCore::CameraDesc& cameraDesc);

///////////////////////////////////////////////////////////////////////////////////////////////////

    void InitDebugDisplays(RenderOverlays::DebuggingDisplay::DebugScreensSystem& system);

}

