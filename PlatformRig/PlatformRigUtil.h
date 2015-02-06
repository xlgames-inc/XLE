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
namespace SceneEngine { class ShadowProjectionDesc; class LightDesc; }
namespace RenderCore { class ProjectionDesc; }

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

    class DefaultShadowFrustumSettings
    {
    public:
        unsigned    _frustumCount;
        bool        _arbitraryCascades;
        float       _maxDistanceFromLight;
        float       _maxDistanceFromCamera;
        DefaultShadowFrustumSettings();
    };

    /// <summary>Calculate a default set of shadow cascades for the sun<summary>
    /// Frequently we render the shadows from the sun using a number of "cascades."
    /// This function will generate reasonable set of cascades given the input parameters
    /// <param name="mainSceneCameraDesc">This is the projection desc used when rendering the 
    /// the main scene from this camera (it's the project desc for the shadows render). This
    /// is required for adapting the shadows projection to the main scene camera.</param>
    SceneEngine::ShadowProjectionDesc CalculateDefaultShadowCascades(
        const SceneEngine::LightDesc& lightDesc,
        const RenderCore::ProjectionDesc& mainSceneCameraDesc,
        const DefaultShadowFrustumSettings& settings);

///////////////////////////////////////////////////////////////////////////////////////////////////

    void InitDebugDisplays(RenderOverlays::DebuggingDisplay::DebugScreensSystem& system);

}

