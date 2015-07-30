// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EnvironmentScene.h"
#include "../Shared/CharactersScene.h"
#include "../Shared/SampleGlobals.h"

#include "../../PlatformRig/PlatformRigUtil.h"

#include "../../SceneEngine/LightDesc.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/PlacementsManager.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/Tonemap.h"
#include "../../SceneEngine/TerrainFormat.h"
#include "../../SceneEngine/TerrainConfig.h"
#include "../../SceneEngine/TerrainMaterial.h"

#include "../../SceneEngine/VolumetricFog.h"
#include "../../SceneEngine/ShallowSurface.h"
#include "../../SceneEngine/VegetationSpawn.h"
#include "../../Tools/EntityInterface/EnvironmentSettings.h"
#include "../../Tools/EntityInterface/RetainedEntities.h"

#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Assets/ModelCache.h"

#include "../../Assets/ConfigFileContainer.h"
#include "../../ConsoleRig/Console.h"
#include "../../Math/Transformations.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/StreamDOM.h"
#include "../../Utility/Profiling/CPUProfiler.h"

namespace Sample
{
    static const char* WorldDirectory = "game/demworld";

    std::shared_ptr<SceneEngine::ITerrainFormat>     MainTerrainFormat;
    SceneEngine::TerrainCoordinateSystem             MainTerrainCoords;
    SceneEngine::TerrainConfig                       MainTerrainConfig;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class IScenePlugin
    {
    public:
        virtual void LoadingPhase() = 0;
        virtual void PrepareFrame(
            RenderCore::IThreadContext& threadContext,
            SceneEngine::LightingParserContext& parserContext) = 0;
        virtual ~IScenePlugin();
    };

    IScenePlugin::~IScenePlugin() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ScenePlugin_EnvironmentFeatures : public IScenePlugin
    {
    public:
        void LoadingPhase();
        void PrepareFrame(
            RenderCore::IThreadContext& threadContext,
            SceneEngine::LightingParserContext& parserContext);

        ScenePlugin_EnvironmentFeatures(std::shared_ptr<EntityInterface::RetainedEntities> retainedEntities);
        ~ScenePlugin_EnvironmentFeatures();

        std::shared_ptr<SceneEngine::VolumetricFogManager>      _volumetricFogMan;
        std::shared_ptr<SceneEngine::VegetationSpawnManager>    _vegetationSpawnManager;
        std::shared_ptr<SceneEngine::ShallowSurfaceManager>     _shallowSurfaces;
        std::shared_ptr<EntityInterface::EnvEntitiesManager>    _updateMan;
    };

    void ScenePlugin_EnvironmentFeatures::LoadingPhase()
    {
        _updateMan->FlushUpdates();
    }

    void ScenePlugin_EnvironmentFeatures::PrepareFrame(
        RenderCore::IThreadContext& threadContext,
        SceneEngine::LightingParserContext& parserContext)
    {
        parserContext._plugins.push_back(_vegetationSpawnManager->GetParserPlugin());
        parserContext._plugins.push_back(_volumetricFogMan->GetParserPlugin());
    }

    ScenePlugin_EnvironmentFeatures::ScenePlugin_EnvironmentFeatures(std::shared_ptr<EntityInterface::RetainedEntities> retainedEntities)
    {
        _volumetricFogMan = std::shared_ptr<SceneEngine::VolumetricFogManager>();
        _shallowSurfaces = std::shared_ptr<SceneEngine::ShallowSurfaceManager>();
        _vegetationSpawnManager = std::shared_ptr<SceneEngine::VegetationSpawnManager>();

            //  Create the object that will manage updates via the entities
            //  interface system.
            //  Then register update events for our scene engine managers.
            //  When relevant game objects are loaded, the entities interface
            //  will push update information through to these managers.
        _updateMan = std::make_shared<EntityInterface::EnvEntitiesManager>(std::move(retainedEntities));
        _updateMan->RegisterVolumetricFogFlexObjects(_volumetricFogMan);
        _updateMan->RegisterShallowSurfaceFlexObjects(_shallowSurfaces);

            // Volume Fog renderer configuration must come from the environment
            // settings... But no access to that here...?
    }

    ScenePlugin_EnvironmentFeatures::~ScenePlugin_EnvironmentFeatures()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class EnvironmentSceneParser::Pimpl
    {
    public:
        std::unique_ptr<CharactersScene>                _characters;
        std::shared_ptr<SceneEngine::TerrainManager>    _terrainManager;
        std::shared_ptr<SceneEngine::PlacementsManager> _placementsManager;
        std::shared_ptr<RenderCore::Techniques::CameraDesc> _cameraDesc;
        PlatformRig::EnvironmentSettings                _envSettings;

        std::shared_ptr<::Assets::DependencyValidation> _terrainCfgVal;
        std::shared_ptr<::Assets::DependencyValidation> _terrainTexturesCfgVal;
        std::shared_ptr<::Assets::DependencyValidation> _placementsCfgVal;
        std::shared_ptr<::Assets::DependencyValidation> _environmentCfgVal;
        std::shared_ptr<::Assets::DependencyValidation> _gameObjectsCfgVal;

        std::shared_ptr<EntityInterface::RetainedEntities> _retainedEntities;

        float _time;

        void LoadGameObjects(const ::Assets::ResChar filename[]);
    };

    void EnvironmentSceneParser::Pimpl::LoadGameObjects(const ::Assets::ResChar filename[])
    {
        size_t fileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(filename, &fileSize);
        using Formatter = InputStreamFormatter<utf8>;
        Formatter formatter(
            MemoryMappedInputStream(sourceFile.get(), PtrAdd(sourceFile.get(), fileSize)));

            // Parse the list of game objects, and create true native objects
            // as required.
            //
            // Note that this path is designed to be generic and flexible...
            // A final solution would do a lot of this work in the "export" step
            // of the editor -- and write out objects that are more closely
            // associated with the native types.
            //
            // But of this sample, we'll just use a simple generic approach.
            // We'll use the EntityInterface library -- this is the library
            // that is used to drive the native objects in the editor.
            // The EntityInterface provides a layer
            // over many native engine types, so they appear as generic
            // containers of names and values called "entities".

        using namespace EntityInterface;
        RetainedEntityInterface interf(_retainedEntities);
        Deserialize(
            formatter, interf, 
            interf.GetDocumentTypeId("GameObjects"));
        // _updateMan->FlushUpdates();

        _gameObjectsCfgVal = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterFileDependency(_gameObjectsCfgVal, filename);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void EnvironmentSceneParser::PrepareFrame(RenderCore::IThreadContext& context)
    {
        ////////////////////////////////////////////////////////////////////////
            // Reload anything that might have changed on disk
        FlushLoading();

        ////////////////////////////////////////////////////////////////////////
            // Culling, etc

        auto metalContext = RenderCore::Metal::DeviceContext::Get(context);
        RenderCore::Metal::ViewportDesc viewport(*metalContext.get());
        auto sceneCamera = GetCameraDesc();
        auto projectionMatrix = RenderCore::Techniques::PerspectiveProjection(
            sceneCamera, viewport.Width / float(viewport.Height));
        auto worldToProjection = Combine(
            InvertOrthonormalTransform(sceneCamera._cameraToWorld), projectionMatrix);

        _pimpl->_characters->Cull(worldToProjection);
        _pimpl->_characters->Prepare(metalContext.get());
    }

    void EnvironmentSceneParser::ExecuteScene(   
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext, 
        const SceneParseSettings& parseSettings,
        unsigned techniqueIndex) const
    {
        CPUProfileEvent pEvnt("ExecuteScene", g_cpuProfiler);

        if (    parseSettings._batchFilter == SceneParseSettings::BatchFilter::General
            ||  parseSettings._batchFilter == SceneParseSettings::BatchFilter::Depth) {

            #if defined(ENABLE_TERRAIN)
                if (parseSettings._toggles & SceneParseSettings::Toggles::Terrain) {
                    if (Tweakable("DoTerrain", true)) {
                        CPUProfileEvent pEvnt("TerrainRender", g_cpuProfiler);
                        _pimpl->_terrainManager->Render(context, parserContext, techniqueIndex);
                    }
                }
            #endif
            
            if (parseSettings._toggles & SceneParseSettings::Toggles::NonTerrain) {
                _pimpl->_characters->Render(context, parserContext, techniqueIndex);

                if (_pimpl->_placementsManager) {
                    CPUProfileEvent pEvnt("PlacementsRender", g_cpuProfiler);
                    _pimpl->_placementsManager->Render(context, parserContext, techniqueIndex);
                }
            }

        }
    }

    void EnvironmentSceneParser::ExecuteShadowScene( 
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext, 
        const SceneParseSettings& parseSettings,
        unsigned frustumIndex, unsigned techniqueIndex) const 
    {
        CPUProfileEvent pEvnt("ExecuteShadowScene", g_cpuProfiler);

        if (Tweakable("DoShadows", true)) {
            SceneParseSettings settings = parseSettings;
            settings._toggles &= ~SceneParseSettings::Toggles::Terrain;
            ExecuteScene(context, parserContext, settings, techniqueIndex);
        }
    }

    RenderCore::Techniques::CameraDesc EnvironmentSceneParser::GetCameraDesc() const 
    { 
        return *_pimpl->_cameraDesc;
    }

    float EnvironmentSceneParser::GetTimeValue() const      
    { 
            //  The scene parser can also provide a time value, in seconds.
            //  This is used to control rendering effects, such as wind
            //  and waves.
        return _pimpl->_time; 
    }

    void EnvironmentSceneParser::Update(float deltaTime)    
    {
        _pimpl->_characters->Update(deltaTime);
        _pimpl->_time += deltaTime; 
    }

    std::shared_ptr<PlayerCharacter>  EnvironmentSceneParser::GetPlayerCharacter()
    {
        return _pimpl->_characters->GetPlayerCharacter();
    }

    std::shared_ptr<SceneEngine::TerrainManager> EnvironmentSceneParser::GetTerrainManager()
    {
        return _pimpl->_terrainManager;
    }

    std::shared_ptr<SceneEngine::PlacementsManager> EnvironmentSceneParser::GetPlacementManager()
    {
        return _pimpl->_placementsManager;
    }

    std::shared_ptr<RenderCore::Techniques::CameraDesc> EnvironmentSceneParser::GetCameraPtr()
    {
        return _pimpl->_cameraDesc;
    }

    const PlatformRig::EnvironmentSettings& EnvironmentSceneParser::GetEnvSettings() const 
    { 
        return _pimpl->_envSettings; 
    }

    // static const ::Assets::ResChar TerrainCfg[] = "TrashWorld/finals/terrain.cfg";
    // static const ::Assets::ResChar PlacementsCfg[] = "TrashWorld/finals/placements.cfg";
    // static const ::Assets::ResChar EnvironmentCfg[] = "TrashWorld/finals/env.txt:environment";
    // static const ::Assets::ResChar GameObjectsCfg[] = "TrashWorld/finals/gameobjects.txt";

    static const ::Assets::ResChar TerrainCfg[] = "DemoWorld2/finals/terrain.cfg";
    static const ::Assets::ResChar TerrainMaterialCfg[] = "DemoWorld2/finals/terrainmaterial.cfg";
    static const ::Assets::ResChar PlacementsCfg[] = "DemoWorld2/finals/placements.cfg";
    static const ::Assets::ResChar EnvironmentCfg[] = "DemoWorld2/finals/env.txt:environment";
    static const ::Assets::ResChar GameObjectsCfg[] = "DemoWorld2/finals/gameobjects.txt";

    // static const Float3 WorldOffset(-11200.f - 7000.f, -11200.f + 700.f, 0.f);
    static const Float3 WorldOffset(-100.f, -100.f, 0.f);

    class EnvironmentSettingsAsset
    {
    public:
        PlatformRig::EnvironmentSettings _settings;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() { return _dependencyValidation; }

        EnvironmentSettingsAsset(const ::Assets::ResChar initializer[]);
        ~EnvironmentSettingsAsset();
    protected:
        std::shared_ptr<::Assets::DependencyValidation> _dependencyValidation;
    };

    EnvironmentSettingsAsset::EnvironmentSettingsAsset(const ::Assets::ResChar initializer[])
    {
        ::Assets::ResChar filename[MaxPath];
        const auto* divider = XlFindChar(initializer, ':');
        if (divider) {
            XlCopyNString(filename, dimof(filename), initializer, divider - initializer);
        } else {
            XlCopyString(filename, initializer);
        }

        size_t fileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(filename, &fileSize);
        using Formatter = InputStreamFormatter<utf8>;
        Formatter formatter(
            MemoryMappedInputStream(sourceFile.get(), PtrAdd(sourceFile.get(), fileSize)));

        auto allSettings = PlatformRig::DeserializeEnvSettings(formatter);
        if (divider) {
            auto i = std::find_if(allSettings.cbegin(), allSettings.cend(),
                [divider](const std::pair<std::string, PlatformRig::EnvironmentSettings>& c)
                {
                    return XlEqString(c.first, (const char*)(divider+1));
                });
            if (i != allSettings.end())
                _settings = i->second;
        } else if (!allSettings.empty()) {
            _settings = allSettings[0].second;
        }

        _dependencyValidation = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterFileDependency(_dependencyValidation, filename);
    }

    EnvironmentSettingsAsset::~EnvironmentSettingsAsset() {}

    void EnvironmentSceneParser::FlushLoading()
    {
        #if defined(ENABLE_TERRAIN)
            if (!_pimpl->_terrainCfgVal || _pimpl->_terrainCfgVal->GetValidationIndex() != 0) {
                ::Assets::ConfigFileContainer<SceneEngine::TerrainConfig> container(TerrainCfg);
                MainTerrainConfig = container._asset;
                _pimpl->_terrainManager->Load(MainTerrainConfig, Int2(0, 0), MainTerrainConfig._cellCount, true);
                _pimpl->_terrainCfgVal = container.GetDependencyValidation();
                MainTerrainCoords = _pimpl->_terrainManager->GetCoords();
            }

            if (!_pimpl->_terrainTexturesCfgVal || _pimpl->_terrainTexturesCfgVal->GetValidationIndex() != 0) {
                ::Assets::ConfigFileContainer<SceneEngine::TerrainMaterialConfig> container(TerrainMaterialCfg);
                _pimpl->_terrainManager->LoadMaterial(container._asset);
                _pimpl->_terrainTexturesCfgVal = container.GetDependencyValidation();
            }
        #endif

        if (!_pimpl->_placementsCfgVal || _pimpl->_placementsCfgVal->GetValidationIndex() != 0) {
            ::Assets::ConfigFileContainer<SceneEngine::WorldPlacementsConfig> container(PlacementsCfg);
            _pimpl->_placementsManager = std::make_shared<SceneEngine::PlacementsManager>(
                container._asset,
                std::make_shared<RenderCore::Assets::ModelCache>(), 
                WorldOffset);
            _pimpl->_placementsCfgVal = container.GetDependencyValidation();
        }

        if (!_pimpl->_environmentCfgVal || _pimpl->_environmentCfgVal->GetValidationIndex() != 0) {
            TRY
            {
                EnvironmentSettingsAsset asset(EnvironmentCfg);
                _pimpl->_envSettings = asset._settings;
                _pimpl->_environmentCfgVal = asset.GetDependencyValidation();
            } CATCH(...) {
            } CATCH_END
        }

        if (!_pimpl->_gameObjectsCfgVal || _pimpl->_gameObjectsCfgVal->GetValidationIndex() != 0) {
            _pimpl->LoadGameObjects(GameObjectsCfg);
        }
    }

    EnvironmentSceneParser::EnvironmentSceneParser()
    {
        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_time = 0.f;
        // pimpl->_characters = std::make_unique<CharactersScene>();

        #if defined(ENABLE_TERRAIN)
            MainTerrainFormat = std::make_shared<SceneEngine::TerrainFormat>();
            pimpl->_terrainManager = std::make_shared<SceneEngine::TerrainManager>(MainTerrainFormat);
            pimpl->_terrainManager->SetWorldSpaceOrigin(WorldOffset);
        #endif

        pimpl->_cameraDesc = std::make_shared<RenderCore::Techniques::CameraDesc>();
        if (pimpl->_characters)
            pimpl->_cameraDesc->_cameraToWorld = pimpl->_characters->DefaultCameraToWorld();
        pimpl->_cameraDesc->_nearClip = 0.5f;
        pimpl->_cameraDesc->_farClip = 6000.f;
        pimpl->_envSettings = PlatformRig::DefaultEnvironmentSettings();

        pimpl->_retainedEntities = std::make_shared<EntityInterface::RetainedEntities>();

        _pimpl = std::move(pimpl);

        FlushLoading();
    }

    EnvironmentSceneParser::~EnvironmentSceneParser()
    {}


}

