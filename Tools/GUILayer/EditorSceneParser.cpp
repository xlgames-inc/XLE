// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LevelEditorScene.h"
#include "EditorInterfaceUtils.h"
#include "IOverlaySystem.h"
#include "MarshalString.h"
#include "ExportedNativeTypes.h"

#include "../EntityInterface/RetainedEntities.h"
#include "../EntityInterface/EnvironmentSettings.h"

#include "../ToolsRig/VisualisationUtils.h"     // for AsCameraDesc
#include "../ToolsRig/ManipulatorsRender.h"
#include "../ToolsRig/ObjectPlaceholders.h"
#include "../../PlatformRig/BasicSceneParser.h"
#include "../../PlatformRig/Screenshot.h"

#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/PlacementsManager.h"
#include "../../SceneEngine/VegetationSpawn.h"
#include "../../SceneEngine/VolumetricFog.h"
#include "../../SceneEngine/ShallowSurface.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/PreparedScene.h"

#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/IThreadContext.h"
#include "../../Utility/StringUtils.h"
#include "../../ConsoleRig/Console.h"

namespace GUILayer
{
    using namespace SceneEngine;
    using EnvironmentSettings = PlatformRig::EnvironmentSettings;

    class EditorSceneParser : public SceneEngine::IScene, public PlatformRig::BasicLightingParserDelegate
    {
    public:
        RenderCore::Techniques::CameraDesc GetCameraDesc() const { return AsCameraDesc(*_camera); }

        using DeviceContext = RenderCore::Metal::DeviceContext;
        using LightingParserContext = SceneEngine::LightingParserContext;

        void ExecuteScene(
            RenderCore::IThreadContext& context, 
			RenderCore::Techniques::ParsingContext& parserContext,
            LightingParserContext& lightingParserContext, 
            RenderCore::Techniques::BatchFilter batchFilter,
            PreparedScene& preparedPackets,
            unsigned techniqueIndex) const;
        void PrepareScene(
            RenderCore::IThreadContext& context, 
			RenderCore::Techniques::ParsingContext& parserContext,
            PreparedScene& preparedPackets) const;

		virtual void ExecuteScene(
            RenderCore::IThreadContext& threadContext,
			SceneExecuteContext& executeContext) const;

        float GetTimeValue() const;
        void PrepareEnvironmentalSettings(const char envSettings[]);
        std::vector<std::shared_ptr<SceneEngine::ILightingParserPlugin>> GetLightingPlugins();

        void RenderShadowForHiddenPlacements(RenderCore::IThreadContext& context, RenderCore::Techniques::ParsingContext& parserContext) const;

        EditorSceneParser(
            std::shared_ptr<EditorScene> editorScene,
            std::shared_ptr<ToolsRig::VisCameraSettings> camera);
        ~EditorSceneParser();
    protected:
        std::shared_ptr<EditorScene> _editorScene;
        std::shared_ptr<ToolsRig::VisCameraSettings> _camera;

        EnvironmentSettings _activeEnvSettings;
        const EnvironmentSettings& GetEnvSettings() const { return _activeEnvSettings; }
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    static ISurfaceHeightsProvider* GetSurfaceHeights(EditorScene& scene)
    {
        return scene._terrainManager ? scene._terrainManager->GetHeightsProvider().get() : nullptr;
    }

    void EditorSceneParser::ExecuteScene(
        RenderCore::IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext, 
		LightingParserContext& lightingParserContext, 
        RenderCore::Techniques::BatchFilter batchFilter,
        PreparedScene& preparedPackets,
        unsigned techniqueIndex) const
    {
        using BF = RenderCore::Techniques::BatchFilter;
        auto& scene = *_editorScene;

        auto& metalContext = *RenderCore::Metal::DeviceContext::Get(context);

        if (1) { // parseSettings._toggles & SceneParseSettings::Toggles::Terrain && scene._terrainManager) {
            if (batchFilter == BF::General) {
                CATCH_ASSETS_BEGIN
                    scene._terrainManager->Render(context, parserContext, this, preparedPackets, techniqueIndex);
                CATCH_ASSETS_END(parserContext)
            }
        }

        if (1) { // parseSettings._toggles & SceneParseSettings::Toggles::NonTerrain) {
            auto delaySteps = SceneEngine::AsDelaySteps(batchFilter);
            CATCH_ASSETS_BEGIN
                for (auto i:delaySteps)
                    if (i != FixedFunctionModel::DelayStep::OpaqueRender) {
                        scene._placementsManager->GetRenderer()->CommitTransparent(
                            metalContext, parserContext, techniqueIndex, i);
                    } else {
                        if (batchFilter == RenderCore::Techniques::BatchFilter::General) {
                            scene._placementsManager->GetRenderer()->Render(
                                metalContext, parserContext, preparedPackets,
                                techniqueIndex, *scene._placementsCells);
                        } else {
                            scene._placementsManager->GetRenderer()->Render(
                                metalContext, parserContext,
                                techniqueIndex, *scene._placementsCells);
                        }
                    }
            CATCH_ASSETS_END(parserContext)

            CATCH_ASSETS_BEGIN
                //for (auto i:delaySteps)
                //    scene._vegetationSpawnManager->Render(context, parserContext, techniqueIndex, i);
            CATCH_ASSETS_END(parserContext)
        
            if (batchFilter == BF::Transparent) {
                CATCH_ASSETS_BEGIN
                    scene._placeholders->Render(metalContext, parserContext, techniqueIndex);
                    scene._shallowSurfaceManager->RenderDebugging(metalContext, parserContext, techniqueIndex, GetSurfaceHeights(scene), lightingParserContext._delegate->GetGlobalLightingDesc());
                CATCH_ASSETS_END(parserContext)
            }
        }
    }

    void EditorSceneParser::PrepareScene(
        RenderCore::IThreadContext& context,
		RenderCore::Techniques::ParsingContext& parserContext, 
        PreparedScene& preparedPackets) const
    {
        auto& scene = *_editorScene;
        if (scene._terrainManager) {
            scene._terrainManager->Prepare(context, parserContext, preparedPackets);
            scene._placementsManager->GetRenderer()->CullToPreparedScene(preparedPackets, parserContext, *scene._placementsCells);
        }
    }

	void EditorSceneParser::ExecuteScene(
        RenderCore::IThreadContext& threadContext,
		SceneExecuteContext& executeContext) const
	{
		assert(0);	// unimplemented
	}

    void EditorSceneParser::RenderShadowForHiddenPlacements(
        RenderCore::IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext) const
    {
        ToolsRig::Placements_RenderHighlight(
            context, parserContext, 
            *_editorScene->_placementsManager->GetRenderer(), 
            *_editorScene->_placementsCellsHidden,
            nullptr, nullptr);
    }

    float EditorSceneParser::GetTimeValue() const { return _editorScene->_currentTime; }

    void EditorSceneParser::PrepareEnvironmentalSettings(const char envSettings[])
    {
        for (const auto& i:_editorScene->_prepareSteps)
            i();

        using namespace EntityInterface;
        const auto& objs = *_editorScene->_flexObjects;
        const RetainedEntity* settings = nullptr;
        const auto typeSettings = objs.GetTypeId((const utf8*)"EnvSettings");

        {
            static const auto nameHash = ParameterBox::MakeParameterNameHash((const utf8*)"Name");
            auto allSettings = objs.FindEntitiesOfType(typeSettings);
            for (const auto& s : allSettings)
                if (!XlCompareStringI(s->_properties.GetString<char>(nameHash).c_str(), envSettings)) {
                    settings = s;
                    break;
                }
        }

        if (settings) {
            _activeEnvSettings = BuildEnvironmentSettings(objs, *settings);
        } else {
            _activeEnvSettings._lights.clear();
            _activeEnvSettings._shadowProj.clear();
            _activeEnvSettings._globalLightingDesc = PlatformRig::DefaultGlobalLightingDesc();
        }
    }

	std::vector<std::shared_ptr<SceneEngine::ILightingParserPlugin>> EditorSceneParser::GetLightingPlugins()
    {
		std::vector<std::shared_ptr<SceneEngine::ILightingParserPlugin>> result;
		auto vegetationPlugin = _editorScene->_vegetationSpawnManager->GetParserPlugin();
		if (vegetationPlugin)
			result.push_back(vegetationPlugin);
        result.push_back(_editorScene->_volumeFogManager->GetParserPlugin());
		return result;
    }

    EditorSceneParser::EditorSceneParser(
        std::shared_ptr<EditorScene> editorScene,
        std::shared_ptr<ToolsRig::VisCameraSettings> camera)
        : _editorScene(std::move(editorScene))
        , _camera(std::move(camera))
    {
        _activeEnvSettings = PlatformRig::DefaultEnvironmentSettings();
    }
    EditorSceneParser::~EditorSceneParser() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    public ref class EditorSceneOverlay : public IOverlaySystem
    {
    public:
        void Render(
            RenderCore::IThreadContext& threadContext,
			const std::shared_ptr<RenderCore::IResource>& renderTarget,
            RenderCore::Techniques::ParsingContext& parserContext) override;

        EditorSceneOverlay(
            std::shared_ptr<EditorSceneParser> sceneParser,
            EditorSceneRenderSettings^ renderSettings,
            std::shared_ptr<SceneEngine::PlacementCellSet> placementCells,
            std::shared_ptr<SceneEngine::PlacementCellSet> placementCellsHidden,
            std::shared_ptr<SceneEngine::PlacementsRenderer> _placementsRenderer);
        ~EditorSceneOverlay();
    protected:
        clix::shared_ptr<EditorSceneParser> _sceneParser;
        EditorSceneRenderSettings^ _renderSettings;
        clix::shared_ptr<SceneEngine::PlacementCellSet> _placementCells;
        clix::shared_ptr<SceneEngine::PlacementCellSet> _placementCellsHidden;
        clix::shared_ptr<SceneEngine::PlacementsRenderer> _placementsRenderer;
    };
    
    void EditorSceneOverlay::Render(
        RenderCore::IThreadContext& threadContext,
		const std::shared_ptr<RenderCore::IResource>& renderTarget,
        RenderCore::Techniques::ParsingContext& parserContext)
    {
        if (_sceneParser.get()) {
            _sceneParser->PrepareEnvironmentalSettings(
                clix::marshalString<clix::E_UTF8>(_renderSettings->_activeEnvironmentSettings).c_str());

			auto lightingPlugins = _sceneParser->GetLightingPlugins();
			auto qualSettings = SceneEngine::RenderSceneSettings{
				(::ConsoleRig::Detail::FindTweakable("LightingModel", 0) == 0)
					? RenderSceneSettings::LightingModel::Deferred 
					: RenderSceneSettings::LightingModel::Forward,
				_sceneParser.get(),
				MakeIteratorRange(lightingPlugins)};

            auto& screenshot = ::ConsoleRig::Detail::FindTweakable("Screenshot", 0);
            if (screenshot) {
                PlatformRig::TiledScreenshot(
                    threadContext, parserContext,
                    *_sceneParser.get(), _sceneParser->GetCameraDesc(),
                    qualSettings, UInt2(screenshot, screenshot));
                screenshot = 0;
            }
            
            SceneEngine::LightingParser_ExecuteScene(
                threadContext, renderTarget, parserContext, *_sceneParser.get(), 
                _sceneParser->GetCameraDesc(), qualSettings);
        }

        if (_renderSettings->_selection && _renderSettings->_selection->_nativePlacements->size() > 0) {
            // Draw a selection highlight for these items
            // at the moment, only placements can be selected... So we need to assume that 
            // they are all placements.
            ToolsRig::Placements_RenderHighlight(
                threadContext, parserContext, 
                *_placementsRenderer.get(), *_placementCells.get(),
                (const SceneEngine::PlacementGUID*)AsPointer(_renderSettings->_selection->_nativePlacements->cbegin()),
                (const SceneEngine::PlacementGUID*)AsPointer(_renderSettings->_selection->_nativePlacements->cend()));
        }

        // render shadow for hidden placements
        if (::ConsoleRig::Detail::FindTweakable("ShadowHiddenPlacements", true)) {
            ToolsRig::Placements_RenderShadow(
                threadContext, parserContext, 
                *_placementsRenderer.get(), *_placementCellsHidden.get());
        }
    }

    EditorSceneOverlay::EditorSceneOverlay(
        std::shared_ptr<EditorSceneParser> sceneParser,
        EditorSceneRenderSettings^ renderSettings,
        std::shared_ptr<SceneEngine::PlacementCellSet> placementCells,
        std::shared_ptr<SceneEngine::PlacementCellSet> placementCellsHidden,
        std::shared_ptr<SceneEngine::PlacementsRenderer> placementsRenderer)
    {
        _sceneParser = std::move(sceneParser);
        _renderSettings = renderSettings;
        _placementCells = std::move(placementCells);
        _placementCellsHidden = std::move(placementCellsHidden);
        _placementsRenderer = std::move(placementsRenderer);
    }
    EditorSceneOverlay::~EditorSceneOverlay() {}


    namespace Internal
    {
        IOverlaySystem^ CreateOverlaySystem(
            std::shared_ptr<EditorScene> scene, 
            std::shared_ptr<ToolsRig::VisCameraSettings> camera, 
            EditorSceneRenderSettings^ renderSettings)
        {
            return gcnew EditorSceneOverlay(
                std::make_shared<EditorSceneParser>(scene, std::move(camera)), 
                renderSettings, scene->_placementsCells, scene->_placementsCellsHidden,
                scene->_placementsManager->GetRenderer());
        }
    }
}

