// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EnvFeatures.h"
#include "SampleGlobals.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/VolumetricFog.h"
#include "../../SceneEngine/ShallowSurface.h"
#include "../../SceneEngine/VegetationSpawn.h"
#include "../../SceneEngine/SceneEngineUtils.h" // for AsDelaySteps
#include "../../RenderCore/GPUProfiler.h"
#include "../../Tools/EntityInterface/EnvironmentSettings.h"
#include "../../Tools/EntityInterface/RetainedEntities.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Utility/Profiling/CPUProfiler.h"

namespace Sample
{
    static const ::Assets::ResChar VegetationSpawnCfg[] = "vegetationspawn.cfg";
    namespace GPUProfiler = RenderCore::Metal::GPUProfiler;

    void ScenePlugin_EnvironmentFeatures::LoadingPhase()
    {
        _updateMan->FlushUpdates();

        if (_vegetationSpawnManager && (!_vegetationSpawnCfgVal || _vegetationSpawnCfgVal->GetValidationIndex() != 0)) {
            ::Assets::ConfigFileContainer<SceneEngine::VegetationSpawnConfig> container((_cfgDir + VegetationSpawnCfg).c_str());
            _vegetationSpawnManager->Load(container._asset);
            _vegetationSpawnCfgVal = container.GetDependencyValidation();
        }
    }

    void ScenePlugin_EnvironmentFeatures::PrepareFrame(
        RenderCore::IThreadContext& threadContext,
        SceneEngine::LightingParserContext& parserContext)
    {
        if (_vegetationSpawnManager)
            parserContext._plugins.push_back(_vegetationSpawnManager->GetParserPlugin());
        parserContext._plugins.push_back(_volumetricFogMan->GetParserPlugin());
    }

    static const char* VegetationSpawnName(SceneEngine::SceneParseSettings::BatchFilter batchFilter)
    {
        using BF = SceneEngine::SceneParseSettings::BatchFilter;
        switch (batchFilter) {
        case BF::General: return "VegetationSpawn-General";
        case BF::Transparent:
        case BF::OITransparent: return "VegetationSpawn-Transparent";
        case BF::PreDepth:
        case BF::TransparentPreDepth: return "VegetationSpawn-PreDepth";
        case BF::DMShadows:
        case BF::RayTracedShadows: return "VegetationSpawn-Shadows";
        default: return "VegetationSpawn-Unknown";
        }
    }

    void ScenePlugin_EnvironmentFeatures::ExecuteScene(   
        RenderCore::Metal::DeviceContext* context, 
        SceneEngine::LightingParserContext& parserContext, 
        const SceneEngine::SceneParseSettings& parseSettings,
        unsigned techniqueIndex) const
    {
        using BF = SceneEngine::SceneParseSettings::BatchFilter;
        using Toggles = SceneEngine::SceneParseSettings::Toggles;

        if (parseSettings._toggles & Toggles::NonTerrain) {
            auto delaySteps = SceneEngine::AsDelaySteps(parseSettings._batchFilter);

            if (!delaySteps.empty()) {
                auto* name = VegetationSpawnName(parseSettings._batchFilter);
                CPUProfileEvent pEvnt(name, g_cpuProfiler);
                RenderCore::GPUProfilerBlock profileBlock(*g_gpuProfiler.get(), *context, name);
                CATCH_ASSETS_BEGIN
                    for (auto i:delaySteps)
                        _vegetationSpawnManager->Render(*context, parserContext, techniqueIndex, i);
                CATCH_ASSETS_END(parserContext)
            }

            if (parseSettings._batchFilter == BF::Transparent) {
                CPUProfileEvent pEvnt("ShallowSurface", g_cpuProfiler);
				RenderCore::GPUProfilerBlock profileBlock(*g_gpuProfiler.get(), *context, "ShallowSurface");
                if (_shallowSurfaces && _surfaceHeights)
                    _shallowSurfaces->RenderDebugging(*context, parserContext, techniqueIndex, _surfaceHeights.get());
            }
        }
    }

    bool ScenePlugin_EnvironmentFeatures::HasContent(const SceneEngine::SceneParseSettings& parseSettings) const
    {
        using BF = SceneEngine::SceneParseSettings::BatchFilter;
        using Toggles = SceneEngine::SceneParseSettings::Toggles;
        if (parseSettings._toggles & Toggles::NonTerrain) {
            if ((parseSettings._batchFilter == BF::Transparent) && _shallowSurfaces)
                return true;

            if (_vegetationSpawnManager) {
                auto delaySteps = SceneEngine::AsDelaySteps(parseSettings._batchFilter);
                for (auto i:delaySteps)
                    if (_vegetationSpawnManager->HasContent(i)) return true;
            }
        }
        return false;
    }

    void ScenePlugin_EnvironmentFeatures::SetSurfaceHeights(
        std::shared_ptr<SceneEngine::ISurfaceHeightsProvider> surfaceHeights)
    {
        _surfaceHeights = std::move(surfaceHeights);
    }

    ScenePlugin_EnvironmentFeatures::ScenePlugin_EnvironmentFeatures(
        const ::Assets::rstring& cfgDir,
        std::shared_ptr<EntityInterface::RetainedEntities> retainedEntities,
        std::shared_ptr<RenderCore::Assets::ModelCache> modelCache)
    : _cfgDir(cfgDir)
    {
        _volumetricFogMan = std::make_shared<SceneEngine::VolumetricFogManager>();
        _shallowSurfaces = std::make_shared<SceneEngine::ShallowSurfaceManager>();
        _vegetationSpawnManager = std::make_shared<SceneEngine::VegetationSpawnManager>(std::move(modelCache));

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

    IScenePlugin::~IScenePlugin() {}

}

