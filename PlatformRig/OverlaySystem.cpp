// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "OverlaySystem.h"
#include "../../PlatformRig/DebuggingDisplays/ConsoleDisplay.h"
#include "../../RenderOverlays/OverlayContext.h"
#include "../../RenderCore/IThreadContext.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/RenderPassUtils.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../Assets/Assets.h"
#include "../../ConsoleRig/Console.h"

namespace PlatformRig
{
    using RenderOverlays::DebuggingDisplay::IInputListener;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class OverlaySystemSwitch::InputListener : public IInputListener
    {
    public:
        virtual bool    OnInputEvent(
			const RenderOverlays::DebuggingDisplay::InputContext& context,
			const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
        {
            using namespace RenderOverlays::DebuggingDisplay;
            static const KeyId shiftKey = KeyId_Make("shift");
            if (evnt.IsHeld(shiftKey)) {
                for (auto i=_parent->_childSystems.cbegin(); i!=_parent->_childSystems.cend(); ++i) {
                    if (evnt.IsPress(i->first)) {
                        auto newIndex = std::distance(_parent->_childSystems.cbegin(), i);

                        if (_parent->_activeChildIndex >= 0 && _parent->_activeChildIndex < signed(_parent->_childSystems.size())) {
                            _parent->_childSystems[_parent->_activeChildIndex].second->SetActivationState(false);
                        }
                            
                        if (signed(newIndex) != _parent->_activeChildIndex) {
                            _parent->_activeChildIndex = signed(newIndex);
                            if (_parent->_activeChildIndex >= 0 && _parent->_activeChildIndex < signed(_parent->_childSystems.size())) {
                                _parent->_childSystems[_parent->_activeChildIndex].second->SetActivationState(true);
                            }
                        } else {
                            _parent->_activeChildIndex = -1;
                        }

                        return true;
                    }
                }
            }

            if (_parent->_activeChildIndex >= 0 && _parent->_activeChildIndex < signed(_parent->_childSystems.size())) {

                    //  if we have an active overlay system, we always consume all input!
                    //  Nothing gets through to the next level
                _parent->_childSystems[_parent->_activeChildIndex].second->GetInputListener()->OnInputEvent(context, evnt);
                return true;
            }

            return false;
        }

        InputListener(OverlaySystemSwitch* parent) : _parent(parent) {}
    protected:
        OverlaySystemSwitch* _parent;
    };

    std::shared_ptr<IInputListener> OverlaySystemSwitch::GetInputListener()
    {
        return _inputListener;
    }

    void OverlaySystemSwitch::Render(
        RenderCore::IThreadContext& device, const RenderCore::IResourcePtr& renderTarget, RenderCore::Techniques::ParsingContext& parserContext) 
    {
        if (_activeChildIndex >= 0 && _activeChildIndex < signed(_childSystems.size())) {
            _childSystems[_activeChildIndex].second->Render(device, renderTarget, parserContext);
        }
    }

    void OverlaySystemSwitch::SetActivationState(bool newState) 
    {
        if (!newState) {
            if (_activeChildIndex >= 0 && _activeChildIndex < signed(_childSystems.size())) {
                _childSystems[_activeChildIndex].second->SetActivationState(false);
            }
            _activeChildIndex = -1;
        }
    }

    void OverlaySystemSwitch::AddSystem(uint32 activator, std::shared_ptr<IOverlaySystem> system)
    {
        _childSystems.push_back(std::make_pair(activator, std::move(system)));
    }

    OverlaySystemSwitch::OverlaySystemSwitch() 
    : _activeChildIndex(-1)
    {
        _inputListener = std::make_shared<InputListener>(this);
    }

    OverlaySystemSwitch::~OverlaySystemSwitch() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class OverlaySystemSet::InputListener : public IInputListener
    {
    public:
        virtual bool    OnInputEvent(
			const RenderOverlays::DebuggingDisplay::InputContext& context,
			const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
        {
            using namespace RenderOverlays::DebuggingDisplay;
            for (auto i=_parent->_childSystems.begin(); i!=_parent->_childSystems.end(); ++i) {
                auto listener = (*i)->GetInputListener();
                if (listener && listener->OnInputEvent(context, evnt)) {
                    return true;
                }
            }

            return false;
        }

        InputListener(OverlaySystemSet* parent) : _parent(parent) {}
    protected:
        OverlaySystemSet* _parent;
    };

    std::shared_ptr<IInputListener> OverlaySystemSet::GetInputListener()         { return _inputListener; }

    void OverlaySystemSet::Render(
        RenderCore::IThreadContext& device,
		const RenderCore::IResourcePtr& renderTarget,
        RenderCore::Techniques::ParsingContext& parsingContext) 
    {
        for (auto i=_childSystems.begin(); i!=_childSystems.end(); ++i) {
            (*i)->Render(device, renderTarget, parsingContext);
        }
    }

    void OverlaySystemSet::SetActivationState(bool newState) 
    {
        for (auto i=_childSystems.begin(); i!=_childSystems.end(); ++i) {
            (*i)->SetActivationState(newState);
        }
    }

    void OverlaySystemSet::AddSystem(std::shared_ptr<IOverlaySystem> system)
    {
        _childSystems.push_back(std::move(system));
            // todo -- do we need to call SetActivationState() here?
    }

	void OverlaySystemSet::RemoveSystem(IOverlaySystem& system)
    {
		for (auto i=_childSystems.begin(); i!=_childSystems.end(); ++i)
			if (i->get() == &system) {
				_childSystems.erase(i);
				return;
			}
	}

    OverlaySystemSet::OverlaySystemSet() 
    : _activeChildIndex(-1)
    {
        _inputListener = std::make_shared<InputListener>(this);
    }

    OverlaySystemSet::~OverlaySystemSet() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	std::shared_ptr<IInputListener> IOverlaySystem::GetInputListener() { return nullptr; }
	void IOverlaySystem::SetActivationState(bool newState) {}
    IOverlaySystem::~IOverlaySystem() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ConsoleOverlaySystem : public IOverlaySystem
    {
    public:
        std::shared_ptr<IInputListener> GetInputListener();
        void Render(
            RenderCore::IThreadContext& device,
			const RenderCore::IResourcePtr& renderTarget,
            RenderCore::Techniques::ParsingContext& parserContext);
        void SetActivationState(bool);

        ConsoleOverlaySystem();
        ~ConsoleOverlaySystem();

    private:
        typedef RenderOverlays::DebuggingDisplay::DebugScreensSystem DebugScreensSystem;
        std::shared_ptr<DebugScreensSystem>     _screens;
    };

    std::shared_ptr<IInputListener> ConsoleOverlaySystem::GetInputListener()
    {
        return _screens;
    }

    void ConsoleOverlaySystem::Render(
        RenderCore::IThreadContext& threadContext,
		const RenderCore::IResourcePtr& renderTarget,
        RenderCore::Techniques::ParsingContext& parserContext)
    {
		auto overlayContext = RenderOverlays::MakeImmediateOverlayContext(threadContext, &parserContext.GetNamedResources(), parserContext.GetProjectionDesc());
		auto viewportDims = threadContext.GetStateDesc()._viewportDimensions;
		auto rpi = RenderCore::Techniques::RenderPassToPresentationTarget(threadContext, renderTarget, parserContext);
		_screens->Render(*overlayContext, RenderOverlays::DebuggingDisplay::Rect{ {0,0}, {int(viewportDims[0]), int(viewportDims[1])} });
    }

    void ConsoleOverlaySystem::SetActivationState(bool) {}

    ConsoleOverlaySystem::ConsoleOverlaySystem()
    {
        _screens = std::make_shared<DebugScreensSystem>();

        auto consoleDisplay = std::make_shared<PlatformRig::Overlays::ConsoleDisplay>(
            std::ref(ConsoleRig::Console::GetInstance()));
        _screens->Register(consoleDisplay, "[Console] Console", DebugScreensSystem::SystemDisplay);
    }

    ConsoleOverlaySystem::~ConsoleOverlaySystem()
    {
    }

    std::shared_ptr<IOverlaySystem> CreateConsoleOverlaySystem()
    {
        return std::make_shared<ConsoleOverlaySystem>();
    }

}

