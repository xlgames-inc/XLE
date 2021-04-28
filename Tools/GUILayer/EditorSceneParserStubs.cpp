// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LevelEditorScene.h"
#include "IOverlaySystem.h"
#include "MarshalString.h"
#include "ExportedNativeTypes.h"

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
        EditorSceneRenderSettings^ _renderSettings;
    };
    
    void EditorSceneOverlay::Render(
        RenderCore::IThreadContext& threadContext,
		const RenderTargetWrapper& renderTarget,
        RenderCore::Techniques::ParsingContext& parserContext)
    {
    }

    EditorSceneOverlay::EditorSceneOverlay(
        const std::shared_ptr<EditorScene>& sceneParser,
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool,
		const std::shared_ptr<ToolsRig::VisCameraSettings>& camera, 
        EditorSceneRenderSettings^ renderSettings)
    {
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

