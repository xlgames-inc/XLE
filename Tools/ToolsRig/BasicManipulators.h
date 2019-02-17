// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderOverlays/DebuggingDisplay.h"
#include <memory>

namespace SceneEngine { class IntersectionTestContext; class IntersectionTestScene; }

namespace ToolsRig
{
    class IManipulator;
    class VisCameraSettings;

    std::shared_ptr<IManipulator> CreateCameraManipulator(std::shared_ptr<VisCameraSettings> visCameraSettings);


    class ManipulatorStack : public RenderOverlays::DebuggingDisplay::IInputListener
    {
    public:
        bool    OnInputEvent(
			const RenderOverlays::DebuggingDisplay::InputContext& context,
			const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt);
        void    Register(uint64 id, std::shared_ptr<ToolsRig::IManipulator> manipulator);

        static const uint64 CameraManipulator = 256;

        ManipulatorStack(
            std::shared_ptr<SceneEngine::IntersectionTestContext> intrContext = nullptr,
            std::shared_ptr<SceneEngine::IntersectionTestScene> intrScene = nullptr);
        ~ManipulatorStack();
    protected:
        std::vector<std::shared_ptr<ToolsRig::IManipulator>> _activeManipulators;
        std::vector<std::pair<uint64, std::shared_ptr<ToolsRig::IManipulator>>> _registeredManipulators;
        std::shared_ptr<SceneEngine::IntersectionTestContext> _intrContext;
        std::shared_ptr<SceneEngine::IntersectionTestScene> _intrScene;
    };

}

