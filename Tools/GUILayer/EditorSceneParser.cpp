// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LevelEditorScene.h"
#include "ObjectPlaceholders.h"
#include "EditorInterfaceUtils.h"
#include "IOverlaySystem.h"
#include "MarshalString.h"
#include "ExportedNativeTypes.h"

#include "../EntityInterface/RetainedEntities.h"
#include "../EntityInterface/EnvironmentSettings.h"

#include "../ToolsRig/VisualisationUtils.h"     // for AsCameraDesc
#include "../ToolsRig/ManipulatorsRender.h"
#include "../../PlatformRig/BasicSceneParser.h"

#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/PlacementsManager.h"

#include "../../RenderCore/IThreadContext.h"
#include "../../Utility/StringUtils.h"

namespace GUILayer
{
    using namespace SceneEngine;
    using EnvironmentSettings = PlatformRig::EnvironmentSettings;

    class EditorSceneParser : public PlatformRig::BasicSceneParser
    {
    public:
        RenderCore::Techniques::CameraDesc GetCameraDesc() const { return AsCameraDesc(*_camera); }

        using DeviceContext = RenderCore::Metal::DeviceContext;
        using LightingParserContext = SceneEngine::LightingParserContext;
        using SceneParseSettings =  SceneEngine::SceneParseSettings;

        void ExecuteScene(  
            DeviceContext* context, 
            LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings,
            unsigned techniqueIndex) const;
        void ExecuteShadowScene(    
            DeviceContext* context, 
            LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings,
            unsigned index, unsigned techniqueIndex) const;

        float GetTimeValue() const;
        void PrepareEnvironmentalSettings(const char envSettings[]);

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

    void EditorSceneParser::ExecuteScene(  
        DeviceContext* context, 
        LightingParserContext& parserContext, 
        const SceneParseSettings& parseSettings,
        unsigned techniqueIndex) const
    {
        if (    parseSettings._batchFilter == SceneParseSettings::BatchFilter::General
            ||  parseSettings._batchFilter == SceneParseSettings::BatchFilter::Depth) {

			if (parseSettings._toggles & SceneParseSettings::Toggles::Terrain && _editorScene->_terrainManager) {
				TRY {
                    _editorScene->_terrainManager->Render(context, parserContext, techniqueIndex);
                }
                CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
                CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
                CATCH_END
			}

            if (parseSettings._toggles & SceneParseSettings::Toggles::NonTerrain) {
                _editorScene->_placementsManager->Render(
                    context, parserContext, techniqueIndex);

                TRY {
                    _editorScene->_placeholders->Render(
                        *context, parserContext, techniqueIndex);
                }
                CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
                CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
                CATCH_END
            }
        }
        else 
        if (parseSettings._batchFilter == SceneParseSettings::BatchFilter::Transparent)
        {
            _editorScene->_placementsManager->RenderTransparent(
                context, parserContext, techniqueIndex);
        }
    }

    void EditorSceneParser::ExecuteShadowScene(    
        DeviceContext* context, 
        LightingParserContext& parserContext, 
        const SceneParseSettings& parseSettings,
        unsigned index, unsigned techniqueIndex) const
    {
            // disable terrain rendering when writing shadow
        auto newSettings = parseSettings;
        newSettings._toggles &= ~SceneParseSettings::Toggles::Terrain;
        ExecuteScene(context, parserContext, newSettings, techniqueIndex);
    }

    float EditorSceneParser::GetTimeValue() const { return _editorScene->_currentTime; }

    void EditorSceneParser::PrepareEnvironmentalSettings(const char envSettings[])
    {
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
        void RenderToScene(
            RenderCore::IThreadContext* threadContext, 
            SceneEngine::LightingParserContext& parserContext) override;
        void RenderWidgets(
            RenderCore::IThreadContext* device, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc) override;
        void SetActivationState(bool newState) override;

        EditorSceneOverlay(
            std::shared_ptr<EditorSceneParser> sceneParser,
            EditorSceneRenderSettings^ renderSettings,
            std::shared_ptr<SceneEngine::PlacementsEditor> placementsEditor);
        ~EditorSceneOverlay();
    protected:
        clix::shared_ptr<EditorSceneParser> _sceneParser;
        EditorSceneRenderSettings^ _renderSettings;
        clix::shared_ptr<SceneEngine::PlacementsEditor> _placementsEditor;
    };
    
    void EditorSceneOverlay::RenderToScene(
        RenderCore::IThreadContext* threadContext, 
        SceneEngine::LightingParserContext& parserContext)
    {
        if (_sceneParser.get()) {
            _sceneParser->PrepareEnvironmentalSettings(
                clix::marshalString<clix::E_UTF8>(_renderSettings->_activeEnvironmentSettings).c_str());
            SceneEngine::LightingParser_ExecuteScene(
                *threadContext, parserContext, *_sceneParser.get(), 
                SceneEngine::RenderingQualitySettings(threadContext->GetStateDesc()._viewportDimensions));
        }

        if (_renderSettings->_selection && _renderSettings->_selection->_nativePlacements->size() > 0) {
            // Draw a selection highlight for these items
            // at the moment, only placements can be selected... So we need to assume that 
            // they are all placements.
            ToolsRig::Placements_RenderHighlight(
                *threadContext, parserContext, _placementsEditor.get(),
                (const SceneEngine::PlacementGUID*)AsPointer(_renderSettings->_selection->_nativePlacements->cbegin()),
                (const SceneEngine::PlacementGUID*)AsPointer(_renderSettings->_selection->_nativePlacements->cend()));
        }
    }

    void EditorSceneOverlay::RenderWidgets(
        RenderCore::IThreadContext*, 
        const RenderCore::Techniques::ProjectionDesc&)
    {}

    void EditorSceneOverlay::SetActivationState(bool) {}
    EditorSceneOverlay::EditorSceneOverlay(
        std::shared_ptr<EditorSceneParser> sceneParser,
        EditorSceneRenderSettings^ renderSettings,
        std::shared_ptr<SceneEngine::PlacementsEditor> placementsEditor)
    {
        _sceneParser = std::move(sceneParser);
        _renderSettings = renderSettings;
        _placementsEditor = std::move(placementsEditor);
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
                renderSettings, scene->_placementsEditor);
        }
    }
}

