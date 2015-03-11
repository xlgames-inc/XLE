// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "OverlaySystem.h"

namespace PlatformRig
{
    class ModelVisLayer : public IOverlaySystem
    {
    public:
        virtual std::shared_ptr<IInputListener> GetInputListener();

        virtual void RenderToScene(
            RenderCore::IThreadContext* context, 
            SceneEngine::LightingParserContext& parserContext); 
        virtual void RenderWidgets(
            RenderCore::IThreadContext* context, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc);
        virtual void SetActivationState(bool newState);

        ModelVisLayer();
        ~ModelVisLayer();

    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}

