// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderOverlays/DebuggingDisplay.h"
#include <memory>

namespace SceneEngine { class IntersectionTestScene; }
namespace RenderCore { namespace Techniques { class TechniqueContext; } }

namespace ToolsRig
{
    class IManipulator;
    class VisCameraSettings;

    std::shared_ptr<IManipulator> CreateCameraManipulator(std::shared_ptr<VisCameraSettings> visCameraSettings);

    class ManipulatorStack : public PlatformRig::IInputListener
    {
    public:
        bool    OnInputEvent(const PlatformRig::InputContext& context, const PlatformRig::InputSnapshot& evnt);
        void    Register(uint64 id, std::shared_ptr<ToolsRig::IManipulator> manipulator);

		void	Set(const std::shared_ptr<SceneEngine::IntersectionTestScene>& intersectionScene);

        static const uint64 CameraManipulator = 256;

        ManipulatorStack(
			const std::shared_ptr<VisCameraSettings>& camera,
			const std::shared_ptr<RenderCore::Techniques::TechniqueContext>& techniqueContext);
        ~ManipulatorStack();
    protected:
        std::vector<std::shared_ptr<ToolsRig::IManipulator>> _activeManipulators;
        std::vector<std::pair<uint64, std::shared_ptr<ToolsRig::IManipulator>>> _registeredManipulators;

        std::shared_ptr<VisCameraSettings> _camera;
		std::shared_ptr<RenderCore::Techniques::TechniqueContext> _techniqueContext;
		std::shared_ptr<SceneEngine::IntersectionTestScene> _intersectionScene;
    };

}

