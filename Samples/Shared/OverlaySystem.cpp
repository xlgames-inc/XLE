// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "OverlaySystem.h"
#include "../../PlatformRig/DebuggingDisplays/ConsoleDisplay.h"
#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../ConsoleRig/Console.h"

namespace Sample
{
    using RenderOverlays::DebuggingDisplay::IInputListener;

    class OverlaySystemManager::InputListener : public IInputListener
    {
    public:
        virtual bool    OnInputEvent(const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
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
                _parent->_childSystems[_parent->_activeChildIndex].second->GetInputListener()->OnInputEvent(evnt);
                return true;
            }

            return false;
        }

        InputListener(OverlaySystemManager* parent) : _parent(parent) {}
    protected:
        OverlaySystemManager* _parent;
    };

    std::shared_ptr<IInputListener> OverlaySystemManager::GetInputListener()
    {
        return _inputListener;
    }

    void OverlaySystemManager::RenderWidgets(RenderCore::IDevice* device, const RenderCore::ProjectionDesc& projectionDesc) 
    {
        if (_activeChildIndex >= 0 && _activeChildIndex < signed(_childSystems.size())) {
            _childSystems[_activeChildIndex].second->RenderWidgets(device, projectionDesc);
        }
    }

    void OverlaySystemManager::RenderToScene(
        RenderCore::Metal::DeviceContext* devContext, 
        SceneEngine::LightingParserContext& parserContext) 
    {
        if (_activeChildIndex >= 0 && _activeChildIndex < signed(_childSystems.size())) {
            _childSystems[_activeChildIndex].second->RenderToScene(devContext, parserContext);
        }
    }

    void OverlaySystemManager::SetActivationState(bool newState) 
    {
        if (!newState) {
            if (_activeChildIndex >= 0 && _activeChildIndex < signed(_childSystems.size())) {
                _childSystems[_activeChildIndex].second->SetActivationState(false);
            }
            _activeChildIndex = -1;
        }
    }

    void OverlaySystemManager::AddSystem(uint32 activator, std::shared_ptr<IOverlaySystem> system)
    {
        _childSystems.push_back(std::make_pair(activator, std::move(system)));
    }

    OverlaySystemManager::OverlaySystemManager() 
    : _activeChildIndex(-1)
    {
        _inputListener = std::make_shared<InputListener>(this);
    }

    OverlaySystemManager::~OverlaySystemManager() {}

    IOverlaySystem::~IOverlaySystem() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ConsoleOverlaySystem : public IOverlaySystem
    {
    public:
        std::shared_ptr<IInputListener> GetInputListener();
        void RenderWidgets(RenderCore::IDevice* device, const RenderCore::ProjectionDesc& projectionDesc);
        void RenderToScene(
            RenderCore::Metal::DeviceContext* devContext, 
            SceneEngine::LightingParserContext& parserContext);
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

    void ConsoleOverlaySystem::RenderWidgets(RenderCore::IDevice* device, const RenderCore::ProjectionDesc& projectionDesc)
    {
        _screens->Render(device, projectionDesc);
    }

    void ConsoleOverlaySystem::RenderToScene(
        RenderCore::Metal::DeviceContext* devContext, 
        SceneEngine::LightingParserContext& parserContext) {}
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

