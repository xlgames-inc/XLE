// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LevelEditorScene.h"
#include "FlexGobInterface.h"
#include "TerrainGobInterface.h"
#include "ObjectPlaceholders.h"
#include "EditorInterfaceUtils.h"
#include "IOverlaySystem.h"
#include "MarshalString.h"
#include "ExportedNativeTypes.h"

#include "../ToolsRig/VisualisationUtils.h"     // for AsCameraDesc
#include "../ToolsRig/ManipulatorsRender.h"

#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/PlacementsManager.h"

#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/IThreadContext.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"

namespace GUILayer
{
    using namespace SceneEngine;

    class EditorSceneParser : public SceneEngine::ISceneParser
    {
    public:
        RenderCore::Techniques::CameraDesc GetCameraDesc() const
        {
            return AsCameraDesc(*_camera);
        }

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

        unsigned GetShadowProjectionCount() const;
        SceneEngine::ShadowProjectionDesc GetShadowProjectionDesc(
            unsigned index, 
            const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const;

        unsigned                        GetLightCount() const;
        const SceneEngine::LightDesc&   GetLightDesc(unsigned index) const;
        SceneEngine::GlobalLightingDesc GetGlobalLightingDesc() const;
        float                           GetTimeValue() const;

        void PrepareEnvironmentalSettings(const char envSettings[], EditorDynamicInterface::FlexObjectType& flexGobInterface);

        EditorSceneParser(
            std::shared_ptr<EditorScene> editorScene,
            std::shared_ptr<ToolsRig::VisCameraSettings> camera);
        ~EditorSceneParser();
    protected:
        std::shared_ptr<EditorScene> _editorScene;
        std::shared_ptr<ToolsRig::VisCameraSettings> _camera;

        std::vector<SceneEngine::LightDesc> _lights;
        std::vector<SceneEngine::ShadowProjectionDesc> _shadowProj;
        SceneEngine::GlobalLightingDesc _globalLightingDesc;

        ::Assets::rstring _skyTextureBuffer;
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

			if (parseSettings._toggles & SceneParseSettings::Toggles::Terrain && _editorScene->_terrainGob && _editorScene->_terrainGob->_terrainManager) {
				TRY {
                    _editorScene->_terrainGob->_terrainManager->Render(context, parserContext, techniqueIndex);
                }
                CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
                CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
                CATCH_END
			}

            if (parseSettings._toggles & SceneParseSettings::Toggles::NonTerrain) {
                TRY {
                    _editorScene->_placementsManager->Render(
                        context, parserContext, techniqueIndex);
                }
                CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
                CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
                CATCH_END

                TRY {
                    _editorScene->_placeholders->Render(
                        *context, parserContext, techniqueIndex);
                }
                CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
                CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
                CATCH_END
            }
        }
    }

    void EditorSceneParser::ExecuteShadowScene(    
        DeviceContext* context, 
        LightingParserContext& parserContext, 
        const SceneParseSettings& parseSettings,
        unsigned index, unsigned techniqueIndex) const
    {
        ExecuteScene(context, parserContext, parseSettings, techniqueIndex);
    }

    unsigned EditorSceneParser::GetShadowProjectionCount() const { return (unsigned)_shadowProj.size(); }

    ShadowProjectionDesc EditorSceneParser::GetShadowProjectionDesc(
        unsigned index, const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const 
    { return _shadowProj[index]; }

    static GlobalLightingDesc DefaultGlobalLightingDesc()
    {
        GlobalLightingDesc result;
        result._ambientLight = .03f * Float3(1.f, 1.f, 1.f);
        result._skyTexture = "game/xleres/DefaultResources/sky/desertsky.jpg";
        result._doToneMap = true;
        return result;
    }

    unsigned           EditorSceneParser::GetLightCount() const { return (unsigned)_lights.size(); }
    const LightDesc&   EditorSceneParser::GetLightDesc(unsigned index) const
    {
        return _lights[index];
    }

    GlobalLightingDesc EditorSceneParser::GetGlobalLightingDesc() const
    {
        return _globalLightingDesc;
    }

    float EditorSceneParser::GetTimeValue() const { return 0.f; }

    void EditorSceneParser::PrepareEnvironmentalSettings(const char envSettings[], EditorDynamicInterface::FlexObjectType& flexGobInterface)
    {
        _lights.clear();
        _shadowProj.clear();
        _globalLightingDesc = DefaultGlobalLightingDesc();

        using namespace EditorDynamicInterface;
        const FlexObjectType::Object* settings = nullptr;

        const auto typeSettings = flexGobInterface.GetTypeId("EnvSettings");
        const auto typeAmbient = flexGobInterface.GetTypeId("AmbientSettings");

        {
            static const auto nameHash = ParameterBox::MakeParameterNameHash("name");
            auto allSettings = flexGobInterface.FindObjectsOfType(typeSettings);
            for (auto s : allSettings) {
                char buffer[MaxPath];
                if (s->_properties.GetParameter(nameHash, buffer, ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::UInt8, dimof(buffer)))) {
                    if (!XlCompareStringI(buffer, envSettings)) {
                        settings = s;
                        break;
                    }
                }
            }
        }

        if (settings) {
            for (auto cid : settings->_children) {
                const auto* child = flexGobInterface.GetObject(settings->_doc, cid);
                if (!child) continue;

                if (child->_type == typeAmbient) {
                    static const auto ambientHash = ParameterBox::MakeParameterNameHash("ambientlight");
                    static const auto skyTextureHash = ParameterBox::MakeParameterNameHash("skytexture");
                    static const auto flagsHash = ParameterBox::MakeParameterNameHash("flags");

                    _globalLightingDesc._ambientLight = child->_properties.GetParameter<Float3>(ambientHash, _globalLightingDesc._ambientLight);

                    auto flags = child->_properties.GetParameter<int>(flagsHash);
                    if (flags.first) {
                        _globalLightingDesc._doToneMap = flags.second & (1<<0);
                    }

                    _skyTextureBuffer = GetRString(child->_properties, skyTextureHash);
                    if (!_skyTextureBuffer.empty()) {
                        _globalLightingDesc._skyTexture = _skyTextureBuffer.c_str();
                    }
                }
            }
        } else {
            LightDesc light;
            light._type = LightDesc::Directional;
            light._lightColour = Float3(1.f, 1.f, 1.f);
            light._negativeLightDirection = Normalize(Float3(-.1f, 0.33f, 1.f));
            light._radius = 10000.f;
            light._shadowFrustumIndex = ~unsigned(0x0);
            _lights.push_back(light);
        }
    }

    EditorSceneParser::EditorSceneParser(
        std::shared_ptr<EditorScene> editorScene,
        std::shared_ptr<ToolsRig::VisCameraSettings> camera)
        : _editorScene(std::move(editorScene))
        , _camera(std::move(camera))
    {
        _globalLightingDesc = DefaultGlobalLightingDesc();
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
            std::shared_ptr<EditorDynamicInterface::FlexObjectType> flexGobInterface,
            ObjectSet^ selection, 
            std::shared_ptr<SceneEngine::PlacementsEditor> placementsEditor);
        ~EditorSceneOverlay();
    protected:
        clix::shared_ptr<EditorSceneParser> _sceneParser;
        ObjectSet^ _selection;
        EditorSceneRenderSettings^ _renderSettings;
        clix::shared_ptr<EditorDynamicInterface::FlexObjectType> _flexGobInterface;
        clix::shared_ptr<SceneEngine::PlacementsEditor> _placementsEditor;
    };
    
    void EditorSceneOverlay::RenderToScene(
        RenderCore::IThreadContext* threadContext, 
        SceneEngine::LightingParserContext& parserContext)
    {
        if (_sceneParser.get()) {
            _sceneParser->PrepareEnvironmentalSettings(
                clix::marshalString<clix::E_UTF8>(_renderSettings->_activeEnvironmentSettings).c_str(),
                *_flexGobInterface.GetNativePtr());
            SceneEngine::LightingParser_ExecuteScene(
                *threadContext, parserContext, *_sceneParser.get(), 
                SceneEngine::RenderingQualitySettings(threadContext->GetStateDesc()._viewportDimensions));
        }

        if (_selection && _selection->_nativePlacements->size() > 0) {
            // Draw a selection highlight for these items
            // at the moment, only placements can be selected... So we need to assume that 
            // they are all placements.
            ToolsRig::Placements_RenderHighlight(
                *threadContext, parserContext, _placementsEditor.get(),
                (const SceneEngine::PlacementGUID*)AsPointer(_selection->_nativePlacements->cbegin()),
                (const SceneEngine::PlacementGUID*)AsPointer(_selection->_nativePlacements->cend()));
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
        std::shared_ptr<EditorDynamicInterface::FlexObjectType> flexGobInterface,
        ObjectSet^ selection, 
        std::shared_ptr<SceneEngine::PlacementsEditor> placementsEditor)
    {
        _sceneParser = std::move(sceneParser);
        _selection = selection;
        _renderSettings = renderSettings;
        _placementsEditor = placementsEditor;
        _flexGobInterface = flexGobInterface;
    }
    EditorSceneOverlay::~EditorSceneOverlay() {}


    namespace Internal
    {
        IOverlaySystem^ CreateOverlaySystem(
            std::shared_ptr<EditorScene> scene, 
            std::shared_ptr<EditorDynamicInterface::FlexObjectType> flexGobInterface,
            ObjectSet^ selection, 
            std::shared_ptr<ToolsRig::VisCameraSettings> camera, 
            EditorSceneRenderSettings^ renderSettings)
        {
            return gcnew EditorSceneOverlay(
                std::make_shared<EditorSceneParser>(scene, std::move(camera)), 
                renderSettings, std::move(flexGobInterface),
                selection, scene->_placementsEditor);
        }
    }
}

