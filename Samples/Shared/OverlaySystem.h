// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/Metal/Forward.h"
#include "../../RenderCore/IDevice_Forward.h"
#include "../../Math/Matrix.h"
#include "../../Core/Types.h"
#include <memory>
#include <vector>

namespace SceneEngine { class LightingParserContext; }
namespace RenderOverlays { namespace DebuggingDisplay { class IInputListener; } }
namespace RenderCore { class ProjectionDesc; }

namespace Sample
{
    class IOverlaySystem
    {
    public:
        typedef RenderOverlays::DebuggingDisplay::IInputListener IInputListener;

        virtual std::shared_ptr<IInputListener> GetInputListener() = 0;

        virtual void RenderToScene(
            RenderCore::Metal::DeviceContext* devContext, 
            SceneEngine::LightingParserContext& parserContext) = 0; 
        virtual void RenderWidgets(RenderCore::IDevice* device, const RenderCore::ProjectionDesc& projectionDesc) = 0;
        virtual void SetActivationState(bool newState) = 0;

        virtual ~IOverlaySystem();
    };

    class OverlaySystemManager : public IOverlaySystem
    {
    public:
        std::shared_ptr<IInputListener> GetInputListener();
        void RenderWidgets(RenderCore::IDevice* device, const RenderCore::ProjectionDesc& projectionDesc);
        void RenderToScene(
            RenderCore::Metal::DeviceContext* devContext, 
            SceneEngine::LightingParserContext& parserContext);
        void SetActivationState(bool newState);

        void AddSystem(uint32 activator, std::shared_ptr<IOverlaySystem> system);

        OverlaySystemManager();
        ~OverlaySystemManager();

    private:
        class InputListener;

        signed _activeChildIndex;
        std::vector<std::pair<uint32,std::shared_ptr<IOverlaySystem>>> _childSystems;
        std::shared_ptr<InputListener> _inputListener;
    };

    std::shared_ptr<IOverlaySystem> CreateConsoleOverlaySystem();
    
}

