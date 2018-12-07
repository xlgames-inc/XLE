// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/IDevice_Forward.h"
#include "../../RenderCore/IThreadContext_Forward.h"
#include "../../Math/Matrix.h"
#include "../../Core/Types.h"
#include <memory>
#include <vector>

namespace RenderOverlays { namespace DebuggingDisplay { class IInputListener; } }
namespace RenderCore { namespace Techniques { class ProjectionDesc; class ParsingContext; } }

namespace PlatformRig
{

///////////////////////////////////////////////////////////////////////////////////////////////////
    class IOverlaySystem
    {
    public:
		virtual void Render(
            RenderCore::IThreadContext& threadContext,
			const RenderCore::IResourcePtr& renderTarget,
			RenderCore::Techniques::ParsingContext& parserContext) = 0; 

        using IInputListener = RenderOverlays::DebuggingDisplay::IInputListener;
        virtual std::shared_ptr<IInputListener> GetInputListener();
        virtual void SetActivationState(bool newState);

        virtual ~IOverlaySystem();
    };

///////////////////////////////////////////////////////////////////////////////////////////////////
    class OverlaySystemSwitch : public IOverlaySystem
    {
    public:
        std::shared_ptr<IInputListener> GetInputListener();

        void Render(
            RenderCore::IThreadContext& threadContext,
			const RenderCore::IResourcePtr& renderTarget,
            RenderCore::Techniques::ParsingContext& parserContext);
        void SetActivationState(bool newState);

        void AddSystem(uint32 activator, std::shared_ptr<IOverlaySystem> system);

        OverlaySystemSwitch();
        ~OverlaySystemSwitch();

    private:
        class InputListener;

        signed _activeChildIndex;
        std::vector<std::pair<uint32,std::shared_ptr<IOverlaySystem>>> _childSystems;
        std::shared_ptr<InputListener> _inputListener;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////
    class OverlaySystemSet : public IOverlaySystem
    {
    public:
        std::shared_ptr<IInputListener> GetInputListener();

        void Render(
            RenderCore::IThreadContext& threadContext,
			const RenderCore::IResourcePtr& renderTarget,
            RenderCore::Techniques::ParsingContext& parserContext);
        void SetActivationState(bool newState);

        void AddSystem(std::shared_ptr<IOverlaySystem> system);

        OverlaySystemSet();
        ~OverlaySystemSet();

    private:
        class InputListener;

        signed _activeChildIndex;
        std::vector<std::shared_ptr<IOverlaySystem>> _childSystems;
        std::shared_ptr<InputListener> _inputListener;
    };

    std::shared_ptr<IOverlaySystem> CreateConsoleOverlaySystem();
    
}

