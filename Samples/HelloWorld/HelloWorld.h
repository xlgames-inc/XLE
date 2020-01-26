// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Shared/SampleRig.h"

namespace SceneEngine { class IScene; }
namespace RenderCore { namespace Techniques { class IPipelineAcceleratorPool; } }

namespace Sample
{
	class SampleLightingDelegate;

	class HelloWorldOverlay : public ISampleOverlay
	{
	public:
		virtual void Render(
            RenderCore::IThreadContext& device,
			const RenderCore::IResourcePtr& renderTarget,
			RenderCore::Techniques::ParsingContext& parserContext) override; 

		virtual void OnUpdate(float deltaTime) override;
		virtual void OnStartup(const SampleGlobals& globals) override;

		virtual std::shared_ptr<PlatformRig::IInputListener> GetInputListener();
	private:
		std::shared_ptr<SceneEngine::IScene> _scene;
		std::shared_ptr<SampleLightingDelegate> _lightingDelegate;
		std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool> _pipelineAcceleratorPool;

		class InputListener;
		std::shared_ptr<InputListener> _inputListener;
	};
}