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
        light._negativeLightDirection = Normalize(Float3(-0.15046243f, 0.97377890f, 0.17063323f));
        light._radius = 10000.f;
        light._shadowFrustumIndex = ~unsigned(0x0);
        light._diffuseColor = Float3(3.2803922f, 2.2372551f, 1.9627452f);
        light._nonMetalSpecularBrightness = 7.5f;
        light._specularColor = Float3(6.7647061f, 6.4117646f, 4.7647061f);
        light._diffuseWideningMax = 2.f;
        light._diffuseWideningMin = 0.5f;
        light._diffuseModel = 1;
        return light;
    }

    SceneEngine::GlobalLightingDesc DefaultGlobalLightingDesc()
    {
        SceneEngine::GlobalLightingDesc result;
        result._ambientLight = Float3(0.013921569f, 0.032941177f, 0.042745098f);
        XlCopyString(result._skyTexture, "Game\\xleres\\defaultresources\\sky\\desertsky.dds");
        result._skyReflectionScale = 8.f;
        result._skyReflectionBlurriness = 2.f;
        result._skyBrightness = 0.33f;
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

        {
            SceneEngine::LightDesc secondaryLight;
            secondaryLight._type = SceneEngine::LightDesc::Directional;
            secondaryLight._negativeLightDirection = Normalize(Float3(0.71622938f, 0.48972201f, -0.49717990f));
            secondaryLight._radius = 10000.f;
            secondaryLight._shadowFrustumIndex = ~unsigned(0x0);
            secondaryLight._diffuseColor = Float3(3.2803922f, 2.2372551f, 1.9627452f);
            secondaryLight._nonMetalSpecularBrightness = 5.f;
            secondaryLight._specularColor = Float3(5.f, 5.f, 5.f);
            secondaryLight._diffuseWideningMax = 2.f;
            secondaryLight._diffuseWideningMin = 0.5f;
            secondaryLight._diffuseModel = 0;
            result._lights.push_back(secondaryLight);

            SceneEngine::LightDesc tertiaryLight;
            tertiaryLight._type = SceneEngine::LightDesc::Directional;
            tertiaryLight._negativeLightDirection = Normalize(Float3(-0.75507462f, -0.62672323f, 0.19256261f));
            tertiaryLight._radius = 10000.f;
            tertiaryLight._shadowFrustumIndex = ~unsigned(0x0);
            tertiaryLight._diffuseColor = Float3(0.13725491f, 0.18666667f, 0.18745099f);
            tertiaryLight._nonMetalSpecularBrightness = 3.5f;
            tertiaryLight._specularColor = Float3(3.5f, 3.5f, 3.5f);
            tertiaryLight._diffuseWideningMax = 2.f;
            tertiaryLight._diffuseWideningMin = 0.5f;
            tertiaryLight._diffuseModel = 0;
            result._lights.push_back(tertiaryLight);
        }

        return std::move(result);
    }
}
