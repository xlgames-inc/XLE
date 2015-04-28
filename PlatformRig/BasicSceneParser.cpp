// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "BasicSceneParser.h"

namespace PlatformRig
{
    using namespace SceneEngine;

    unsigned BasicSceneParser::GetShadowProjectionCount() const { return (unsigned)GetEnvSettings()._shadowProj.size(); }
    auto BasicSceneParser::GetShadowProjectionDesc(
        unsigned index, const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const 
        -> ShadowProjectionDesc
    {
        return PlatformRig::CalculateDefaultShadowCascades(
            GetEnvSettings()._shadowProj[index]._light,
            mainSceneProjectionDesc,
            GetEnvSettings()._shadowProj[index]._shadowFrustumSettings);
    }

    unsigned BasicSceneParser::GetLightCount() const { return (unsigned)GetEnvSettings()._lights.size(); }
    auto BasicSceneParser::GetLightDesc(unsigned index) const -> const LightDesc&
    {
        return GetEnvSettings()._lights[index];
    }

    auto BasicSceneParser::GetGlobalLightingDesc() const -> GlobalLightingDesc
    {
        return GetEnvSettings()._globalLightingDesc;
    }

    ToneMapSettings BasicSceneParser::GetToneMapSettings() const
    {
        return GetEnvSettings()._toneMapSettings;
    }



    SceneEngine::LightDesc DefaultDominantLight()
    {
        SceneEngine::LightDesc light;
        light._type = SceneEngine::LightDesc::Directional;
        light._negativeLightDirection = Normalize(Float3(-.1f, 0.33f, 1.f));
        light._radius = 10000.f;
        light._shadowFrustumIndex = ~unsigned(0x0);
        light._diffuseColor = 3.f * Float3(1.f, 1.f, 1.f);
        light._nonMetalSpecularBrightness = 5.f;
        light._specularColor = 30.f * Float3(1.f, 1.f, 1.f);
        return light;
    }

    SceneEngine::GlobalLightingDesc DefaultGlobalLightingDesc()
    {
        SceneEngine::GlobalLightingDesc result;
        result._ambientLight = .33f * Float3(1.f, 1.f, 1.f);
        XlCopyString(result._skyTexture, "Game\\xleres\\defaultresources\\sky\\desertsky.dds");
        result._skyReflectionScale = 1.f;
        result._doAtmosphereBlur = false;
        result._doOcean = false;
        return result;
    }

    EnvironmentSettings DefaultEnvironmentSettings()
    {
        EnvironmentSettings result;
        result._globalLightingDesc = DefaultGlobalLightingDesc();
        result._toneMapSettings = DefaultToneMapSettings();

        auto defLight = DefaultDominantLight();
        defLight._shadowFrustumIndex = 0;
        result._lights.push_back(defLight);

        auto frustumSettings = PlatformRig::DefaultShadowFrustumSettings();
        result._shadowProj.push_back(EnvironmentSettings::ShadowProj { defLight, frustumSettings });

        return std::move(result);
    }
}
