// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LevelEditorScene.h"
#include "IOverlaySystem.h"
#include "MarshalString.h"
#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "ExportedNativeTypes.h"

#include "../ToolsRig/ObjectPlaceholders.h"
#include "../../SceneEngine/PlacementsManager.h"

#include "../../RenderCore/LightingEngine/LightingEngine.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/TechniqueDelegates.h"
#include "../../RenderCore/Techniques/PipelineAccelerator.h"
#include "../../RenderCore/Techniques/Apparatuses.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/IDevice.h"

#include "../ToolsRig/VisualisationUtils.h"     // for AsCameraDesc

namespace GUILayer
{
    using namespace SceneEngine;
 
//////////////////////////////////////////////////////////////////////////////////////////////////

    public ref class EditorSceneOverlay : public IOverlaySystem
    {
    public:
        void Render(
            RenderCore::IThreadContext& threadContext,
			const RenderTargetWrapper& renderTarget,
            RenderCore::Techniques::ParsingContext& parserContext) override;

        EditorSceneOverlay(
            const std::shared_ptr<EditorScene>& sceneParser,
			const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool,
			const std::shared_ptr<ToolsRig::VisCameraSettings>& camera, 
            EditorSceneRenderSettings^ renderSettings);
        ~EditorSceneOverlay();
    protected:
        clix::shared_ptr<EditorScene> _scene;
		clix::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool> _pipelineAcceleratorPool;
		clix::shared_ptr<ToolsRig::VisCameraSettings> _camera;
		clix::shared_ptr<RenderCore::LightingEngine::LightingEngineApparatus> _lightingApparatus;
        EditorSceneRenderSettings^ _renderSettings;
    };

	static void BuildDrawables(
		EditorScene& scene,
		ToolsRig::VisCameraSettings& camera,
		UInt2 viewportDims,
		RenderCore::Techniques::BatchFilter batchFilter,
		RenderCore::Techniques::DrawablesPacket& pkt)
	{
		SceneEngine::ExecuteSceneContext exeContext;
		exeContext._batchFilter = batchFilter;
		exeContext._destinationPkt = &pkt;
		auto camDesc = ToolsRig::AsCameraDesc(camera);
		exeContext._view = SceneEngine::SceneView { SceneEngine::SceneView::Type::Normal, RenderCore::Techniques::BuildProjectionDesc(camDesc, viewportDims) };
		scene._placementsManager->GetRenderer()->BuildDrawables(exeContext, *scene._placementsCells);
		scene._placeholders->BuildDrawables(exeContext);
	}
    
    void EditorSceneOverlay::Render(
        RenderCore::IThreadContext& threadContext,
		const RenderTargetWrapper& renderTargetWrapper,
        RenderCore::Techniques::ParsingContext& parserContext)
    {
		auto& renderTarget = renderTargetWrapper._renderTarget;
        auto targetDesc = renderTarget->GetDesc();
		UInt2 viewportDims { targetDesc._textureDesc._width, targetDesc._textureDesc._height };

		RenderCore::Techniques::PreregisteredAttachment preregisteredAttachments[] {
			RenderCore::Techniques::PreregisteredAttachment {
				RenderCore::Techniques::AttachmentSemantics::ColorLDR,
				RenderCore::AsAttachmentDesc(targetDesc),
				RenderCore::Techniques::PreregisteredAttachment::State::Uninitialized
			}
		};
		RenderCore::FrameBufferProperties fbProps { targetDesc._textureDesc._width, targetDesc._textureDesc._height };

		auto compiledTechnique = RenderCore::LightingEngine::CreateForwardLightingTechnique(
			_pipelineAcceleratorPool.GetNativePtr(), _lightingApparatus.GetNativePtr(),
			MakeIteratorRange(preregisteredAttachments), fbProps);

        {
			ToolsRig::ConfigureParsingContext(parserContext, *_camera.get(), UInt2{targetDesc._textureDesc._width, targetDesc._textureDesc._height});
			
			parserContext.GetTechniqueContext()._attachmentPool->Bind(RenderCore::Techniques::AttachmentSemantics::ColorLDR, renderTarget);
			{
				RenderCore::LightingEngine::SceneLightingDesc lightingDesc;
				auto lightingIterator = RenderCore::LightingEngine::LightingTechniqueInstance{
					threadContext, parserContext, *_pipelineAcceleratorPool.get(),
					lightingDesc, *compiledTechnique};

				for (;;) {
					auto next = lightingIterator.GetNextStep();
					if (next._type == RenderCore::LightingEngine::StepType::None || next._type == RenderCore::LightingEngine::StepType::Abort) break;
					assert(next._type == RenderCore::LightingEngine::StepType::ParseScene);
					assert(next._pkt);
					BuildDrawables(*_scene.get(), *_camera.get(), viewportDims, next._batch, *next._pkt);
				}
			}
			parserContext.GetTechniqueContext()._attachmentPool->Unbind(*renderTarget);
		}
    }

    EditorSceneOverlay::EditorSceneOverlay(
        const std::shared_ptr<EditorScene>& sceneParser,
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool,
		const std::shared_ptr<ToolsRig::VisCameraSettings>& camera, 
        EditorSceneRenderSettings^ renderSettings)
    {
        _scene = sceneParser;
		_pipelineAcceleratorPool = pipelineAcceleratorPool;
		_camera = camera;
        _renderSettings = renderSettings;
		_lightingApparatus = EngineDevice::GetInstance()->GetNative().GetLightingEngineApparatus();
    }

    EditorSceneOverlay::~EditorSceneOverlay() {}


    namespace Internal
    {
        IOverlaySystem^ CreateOverlaySystem(
            const std::shared_ptr<EditorScene>& scene, 
			const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool,
            const std::shared_ptr<ToolsRig::VisCameraSettings>& camera, 
            EditorSceneRenderSettings^ renderSettings)
        {
            return gcnew EditorSceneOverlay(
                scene, 
				pipelineAcceleratorPool,
				camera,
                renderSettings);
        }
    }
}

