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
#include "../../PlatformRig/PlatformRigUtil.h"

#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/PlacementsManager.h"

#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/IThreadContext.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../Math/Transformations.h"

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
        SceneEngine::GlobalLightingDesc _globalLightingDesc;

        class ShadowProj
        {
        public:
            SceneEngine::LightDesc _light;
            PlatformRig::DefaultShadowFrustumSettings _shadowFrustumSettings;
        };
        std::vector<ShadowProj> _shadowProj;
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
            // disable terrain rendering when writing shadow
        auto newSettings = parseSettings;
        newSettings._toggles &= ~SceneParseSettings::Toggles::Terrain;
        ExecuteScene(context, parserContext, newSettings, techniqueIndex);
    }

    unsigned EditorSceneParser::GetShadowProjectionCount() const { return (unsigned)_shadowProj.size(); }

    ShadowProjectionDesc EditorSceneParser::GetShadowProjectionDesc(
        unsigned index, const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const 
    {
        return PlatformRig::CalculateDefaultShadowCascades(
            _shadowProj[index]._light,
            mainSceneProjectionDesc,
            _shadowProj[index]._shadowFrustumSettings);
    }

    static GlobalLightingDesc DefaultGlobalLightingDesc()
    {
        GlobalLightingDesc result;
        result._ambientLight = .03f * Float3(1.f, 1.f, 1.f);
        XlCopyString(result._skyTexture, "game/xleres/DefaultResources/sky/desertsky.dds");
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

    static Float3 AsFloat3Color(unsigned packedColor)
    {
        return Float3(
            (float)((packedColor >> 16) & 0xff) / 255.f,
            (float)((packedColor >>  8) & 0xff) / 255.f,
            (float)(packedColor & 0xff) / 255.f);
    }

    void EditorSceneParser::PrepareEnvironmentalSettings(const char envSettings[], EditorDynamicInterface::FlexObjectType& flexGobInterface)
    {
        _lights.clear();
        _shadowProj.clear();
        _globalLightingDesc = DefaultGlobalLightingDesc();

        using namespace EditorDynamicInterface;
        const FlexObjectType::Object* settings = nullptr;

        const auto typeSettings = flexGobInterface.GetTypeId("EnvSettings");
        const auto typeAmbient = flexGobInterface.GetTypeId("AmbientSettings");
        const auto typeDirectionalLight = flexGobInterface.GetTypeId("DirectionalLight");

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
                    _globalLightingDesc = GlobalLightingDesc(child->_properties);
                }

                if (child->_type == typeDirectionalLight) {
                    static const auto diffuseHash = ParameterBox::MakeParameterNameHash("diffuse");
                    static const auto diffuseBrightnessHash = ParameterBox::MakeParameterNameHash("diffusebrightness");
                    static const auto specularHash = ParameterBox::MakeParameterNameHash("specular");
                    static const auto specularBrightnessHash = ParameterBox::MakeParameterNameHash("specularbrightness");
                    static const auto specularNonMetalBrightnessHash = ParameterBox::MakeParameterNameHash("specularnonmetalbrightness");
                    static const auto flagsHash = ParameterBox::MakeParameterNameHash("flags");
                    static const auto transformHash = ParameterBox::MakeParameterNameHash("Transform");

                    const auto& props = child->_properties;

                    auto transform = Transpose(props.GetParameter(transformHash, Identity<Float4x4>()));
                    auto translation = ExtractTranslation(transform);

                    LightDesc light;
                    light._type = LightDesc::Directional;
                    light._diffuseColor = props.GetParameter(diffuseBrightnessHash, 1.f) * AsFloat3Color(props.GetParameter(diffuseHash, ~0x0u));
                    light._specularColor = props.GetParameter(specularBrightnessHash, 1.f) * AsFloat3Color(props.GetParameter(specularHash, ~0x0u));
                    light._nonMetalSpecularBrightness = props.GetParameter(specularNonMetalBrightnessHash, 1.f);
                    light._negativeLightDirection = (MagnitudeSquared(translation) > 0.01f) ? Normalize(translation) : Float3(0.f, 0.f, 0.f);
                    light._radius = 10000.f;
                    light._shadowFrustumIndex = ~unsigned(0x0);

                    if (props.GetParameter(flagsHash, 0u) & (1<<0)) {
                        light._shadowFrustumIndex = (unsigned)_shadowProj.size();
                        _shadowProj.push_back(
                            ShadowProj { light, PlatformRig::DefaultShadowFrustumSettings() });
                    }

                    _lights.push_back(light);
                }
            }
        } else {
            _lights.push_back(LightDesc());
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

