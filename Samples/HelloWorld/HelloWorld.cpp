// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "HelloWorld.h"
#include "BasicScene.h"
#include "../Shared/SampleRig.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/Techniques/RenderPassUtils.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../ConsoleRig/Console.h"

namespace Sample
{
	void RenderPostScene(RenderCore::IThreadContext& context);

	void HelloWorldOverlay::Render(
        RenderCore::IThreadContext& threadContext,
		const RenderCore::IResourcePtr& renderTarget,
		RenderCore::Techniques::ParsingContext& parsingContext)
	{
        SceneEngine::LightingParserContext lightingParserContext;
		if (_scene) {
			auto samples = RenderCore::TextureSamples::Create((uint8)Tweakable("SamplingCount", 1), (uint8)Tweakable("SamplingQuality", 0));
			// auto stdPlugin = std::make_shared<SceneEngine::LightingParserStandardPlugin>();
            lightingParserContext = LightingParser_ExecuteScene(
                threadContext, renderTarget, parsingContext, *_scene, _lightingDelegate->GetCameraDesc(),
                SceneEngine::RenderSceneSettings {
                    (Tweakable("LightingModel", 0) == 0) ? SceneEngine::RenderSceneSettings::LightingModel::Deferred : SceneEngine::RenderSceneSettings::LightingModel::Forward,
					_lightingDelegate.get(),
					{},
					samples._sampleCount, samples._samplingQuality } );
        }

		{
			auto rpi = RenderCore::Techniques::RenderPassToPresentationTarget(threadContext, renderTarget, parsingContext);
			RenderPostScene(threadContext);
			SceneEngine::LightingParser_Overlays(threadContext, parsingContext, lightingParserContext);
		}
	}

	void HelloWorldOverlay::OnUpdate(float deltaTime)
	{
		_lightingDelegate->Update(deltaTime);
	}

	void HelloWorldOverlay::OnStartup(const SampleGlobals& globals)
	{
		_scene = std::make_shared<BasicSceneParser>();
		_lightingDelegate = std::make_shared<SampleLightingDelegate>();
	}
    
}

