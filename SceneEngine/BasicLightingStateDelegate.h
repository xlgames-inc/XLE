// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ShadowConfiguration.h"
#include "IScene.h"
#include "../RenderCore/LightingEngine/LightDesc.h"
#include "../SceneEngine/Tonemap.h"
#include "../Assets/DepVal.h"
#include "../Assets/AssetsCore.h"
#if 0
#include "../SceneEngine/VolumetricFog.h"
#include "../SceneEngine/Ocean.h"
#include "../SceneEngine/DeepOceanSim.h"
#endif

namespace Utility
{
    template<typename Type> class InputStreamFormatter;
}
namespace Assets { class DirectorySearchRules; }

namespace SceneEngine
{
    /// <summary>Describes a lighting environment</summary>
    /// This contains all of the settings and properties required
    /// for constructing a basic lighting environment.
    /// This can be used to implement the ISceneParser functions that
    /// return lighting settings (like ISceneParser::GetLightDesc() and ISceneParser::GetSceneLightingDesc())
    class EnvironmentSettings
    {
    public:
        std::vector<RenderCore::LightingEngine::LightDesc> _lights;
        RenderCore::LightingEngine::EnvironmentalLightingDesc _environmentalLightingDesc;
        ToneMapSettings _toneMapSettings;

        class ShadowProj
        {
        public:
            RenderCore::LightingEngine::LightDesc _light;
            RenderCore::LightingEngine::LightId _lightId;
            DefaultShadowFrustumSettings _shadowFrustumSettings;
        };
        std::vector<ShadowProj> _shadowProj;

#if 0
        VolumetricFogConfig::Renderer _volFogRenderer;
        OceanLightingSettings _oceanLighting;
        DeepOceanSimSettings _deepOceanSim;
#endif
        
        EnvironmentSettings();
        EnvironmentSettings(
            InputStreamFormatter<utf8>& formatter,
            const ::Assets::DirectorySearchRules&,
			const ::Assets::DependencyValidation&);
        ~EnvironmentSettings();

		const ::Assets::DependencyValidation& GetDependencyValidation() const { return _depVal; }

	private:
		::Assets::DependencyValidation _depVal;
    };

    /// <summary>Simple & partial implementation of the ILightingStateDelegate interface<summary>
    /// This provides implementations of the basic lighting related interfaces of
    /// ISceneParser that will hook into an EnvironmentSettings object.
    /// Derived classes should implement the accessor GetEnvSettings().
    class BasicLightingStateDelegate : public ILightingStateDelegate
    {
    public:
        unsigned    GetShadowProjectionCount() const;
        auto        GetShadowProjectionDesc(
            ShadowProjIndex index, 
            const ProjectionDesc& mainSceneProj) const
            -> RenderCore::LightingEngine::ShadowProjectionDesc;

        unsigned    GetLightCount() const;
        auto        GetLightDesc(unsigned index) const -> const RenderCore::LightingEngine::LightDesc&;
        auto        GetEnvironmentalLightingDesc() const -> RenderCore::LightingEngine::EnvironmentalLightingDesc;
        auto        GetToneMapSettings() const -> ToneMapSettings;

		BasicLightingStateDelegate(
			const std::shared_ptr<EnvironmentSettings>& envSettings);
		~BasicLightingStateDelegate();

		static void ConstructToFuture(
			::Assets::AssetFuture<BasicLightingStateDelegate>& future,
			StringSection<::Assets::ResChar> envSettingFileName);

		const ::Assets::DependencyValidation& GetDependencyValidation() const { return _envSettings->GetDependencyValidation(); }

    protected:
        const EnvironmentSettings&  GetEnvSettings() const;
		std::shared_ptr<EnvironmentSettings>	_envSettings;
    };

    RenderCore::LightingEngine::LightDesc                   DefaultDominantLight();
    RenderCore::LightingEngine::EnvironmentalLightingDesc   DefaultEnvironmentalLightingDesc();
    EnvironmentSettings                                     DefaultEnvironmentSettings();

    RenderCore::LightingEngine::EnvironmentalLightingDesc MakeEnvironmentalLightingDesc(const ParameterBox& props);
    RenderCore::LightingEngine::LightDesc MakeLightDesc(const Utility::ParameterBox& props);
}

