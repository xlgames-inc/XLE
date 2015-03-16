// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once
#include "../../PlatformRig/OverlaySystem.h"

namespace GUILayer 
{
    public ref class IOverlaySystem abstract
    {
    public:
        typedef RenderOverlays::DebuggingDisplay::IInputListener IInputListener;

        virtual std::shared_ptr<IInputListener> GetInputListener() = 0;

        virtual void RenderToScene(
            RenderCore::IThreadContext* device, 
            SceneEngine::LightingParserContext& parserContext) = 0; 
        virtual void RenderWidgets(
            RenderCore::IThreadContext* device, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc) = 0;
        virtual void SetActivationState(bool newState) = 0;

        virtual ~IOverlaySystem() {}
    };
}

