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

#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Assets/ModelCache.h"

#include "../../ConsoleRig/Console.h"
#include "../../Math/Transformations.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Profiling/CPUProfiler.h"

namespace SceneEngine { extern float SunDirectionAngle; }

namespace Sample
{
    static const char* WorldDirectory = "game/demworld";

    std::shared_ptr<SceneEngine::ITerrainFormat>     MainTerrainFormat;
    SceneEngine::TerrainCoordinateSystem             MainTerrainCoords;
    SceneEngine::TerrainConfig                       MainTerrainConfig;

    class EnvironmentSceneParser::Pimpl
    {
    public:
        std::unique_ptr<CharactersScene>                _characters;
        std::shared_ptr<SceneEngine::TerrainManager>    _terrainManager;
        std::shared_ptr<SceneEngine::PlacementsManager> _placementsManager;
        std::shared_ptr<RenderCore::Techniques::CameraDesc> _cameraDesc;
        PlatformRig::EnvironmentSettings _envSettings;

        float _time;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    void EnvironmentSceneParser::PrepareFrame(RenderCore::IThreadContext& context)
    {
        auto metalContext = RenderCore::Metal::DeviceContext::Get(context);
        RenderCore::Metal::ViewportDesc viewport(*metalContext.get());
        auto sceneCamera = GetCameraDesc();
        auto projectionMatrix = RenderCore::Techniques::PerspectiveProjection(
            sceneCamera, viewport.Width / float(viewport.Height));
        auto worldToProjection = Combine(InvertOrthonormalTransform(sceneCamera._cameraToWorld), projectionMatrix);

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

                if (_pimpl->_placementsManager && Tweakable("DoPlacements", true)) {
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

    EnvironmentSceneParser::EnvironmentSceneParser()
    {
        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_time = 0.f;
        pimpl->_characters = std::make_unique<CharactersScene>();

        Float3 worldOffset(-11200.f - 7000.f, -11200.f + 700.f, 0.f);

        #if defined(ENABLE_TERRAIN)
            MainTerrainFormat = std::make_shared<SceneEngine::TerrainFormat>();
            MainTerrainConfig = SceneEngine::TerrainConfig(WorldDirectory);
            pimpl->_terrainManager = std::make_shared<SceneEngine::TerrainManager>(MainTerrainFormat);
            pimpl->_terrainManager->SetWorldSpaceOrigin(worldOffset);
            pimpl->_terrainManager->Load(MainTerrainConfig, Int2(0, 0), MainTerrainConfig._cellCount, true);
            MainTerrainCoords = pimpl->_terrainManager->GetCoords();
        #endif

        pimpl->_placementsManager = std::make_shared<SceneEngine::PlacementsManager>(
            SceneEngine::WorldPlacementsConfig(WorldDirectory),
            std::make_shared<RenderCore::Assets::ModelCache>(), 
            Truncate(worldOffset));

        pimpl->_cameraDesc = std::make_shared<RenderCore::Techniques::CameraDesc>();
        pimpl->_cameraDesc->_cameraToWorld = pimpl->_characters->DefaultCameraToWorld();
        pimpl->_cameraDesc->_nearClip = 0.5f;
        pimpl->_cameraDesc->_farClip = 6000.f;

        pimpl->_envSettings = PlatformRig::DefaultEnvironmentSettings();

        _pimpl = std::move(pimpl);
    }

    EnvironmentSceneParser::~EnvironmentSceneParser()
    {}


}

