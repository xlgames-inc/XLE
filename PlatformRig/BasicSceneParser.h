// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "PlatformRigUtil.h"
#include "../SceneEngine/LightDesc.h"
#include "../SceneEngine/SceneParser.h"
#include "../SceneEngine/ToneMap.h"
#include "../SceneEngine/VolumetricFog.h"
#include "../SceneEngine/Ocean.h"
#include "../SceneEngine/DeepOceanSim.h"

namespace Utility
{
    template<typename Type> class InputStreamFormatter;
}

namespace PlatformRig
{
    /// <summary>Describes a lighting environment</summary>
    /// This contains all of the settings and properties required
    /// for constructing a basic lighting environment.
    /// This can be used to implement the ISceneParser functions that
    /// return lighting settings (like ISceneParser::GetLightDesc() and ISceneParser::GetGlobalLightingDesc())
    class EnvironmentSettings
    {
    public:
        std::vector<SceneEngine::LightDesc> _lights;
        SceneEngine::GlobalLightingDesc _globalLightingDesc;
        SceneEngine::ToneMapSettings _toneMapSettings;

        class ShadowProj
        {
        public:
            SceneEngine::LightDesc _light;
            SceneEngine::LightId _lightId;
            DefaultShadowFrustumSettings _shadowFrustumSettings;
        };
        std::vector<ShadowProj> _shadowProj;

        SceneEngine::VolumetricFogConfig::Renderer _volFogRenderer;
        SceneEngine::OceanLightingSettings _oceanLighting;
        SceneEngine::DeepOceanSimSettings _deepOceanSim;
        
        EnvironmentSettings();
        EnvironmentSettings(
            InputStreamFormatter<utf8>& formatter,
            const ::Assets::DirectorySearchRules&,
			const ::Assets::DepValPtr&);
        ~EnvironmentSettings();

		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

	private:
		::Assets::DepValPtr _depVal;
    };

    /// <summary>Simple & partial implementation of the ILightingParserDelegate interface<summary>
    /// This provides implementations of the basic lighting related interfaces of
    /// ISceneParser that will hook into an EnvironmentSettings object.
    /// Derived classes should implement the accessor GetEnvSettings().
    class BasicLightingParserDelegate : public SceneEngine::ILightingParserDelegate
    {
    public:
        unsigned    GetShadowProjectionCount() const;
        auto        GetShadowProjectionDesc(
            ShadowProjIndex index, 
            const ProjectionDesc& mainSceneProj) const
            -> SceneEngine::ShadowProjectionDesc;

        unsigned    GetLightCount() const;
        auto        GetLightDesc(unsigned index) const -> const SceneEngine::LightDesc&;
        auto        GetGlobalLightingDesc() const -> SceneEngine::GlobalLightingDesc;
        auto        GetToneMapSettings() const -> SceneEngine::ToneMapSettings;

    protected:
        virtual const EnvironmentSettings&  GetEnvSettings() const = 0;
    };

    SceneEngine::LightDesc          DefaultDominantLight();
    SceneEngine::GlobalLightingDesc DefaultGlobalLightingDesc();
    EnvironmentSettings             DefaultEnvironmentSettings();
}

