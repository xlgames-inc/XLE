// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "BasicSceneParser.h"
#include "../../Math/Transformations.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Conversion.h"
#include "../../Utility/Meta/AccessorSerialize.h"

namespace PlatformRig
{
    using namespace SceneEngine;

    unsigned BasicSceneParser::GetShadowProjectionCount() const 
    { 
        return (unsigned)GetEnvSettings()._shadowProj.size(); 
    }

    auto BasicSceneParser::GetShadowProjectionDesc(
        unsigned index, const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const 
        -> ShadowProjectionDesc
    {
        return PlatformRig::CalculateDefaultShadowCascades(
            GetEnvSettings()._shadowProj[index]._light, 
            GetEnvSettings()._shadowProj[index]._lightId,
            mainSceneProjectionDesc,
            GetEnvSettings()._shadowProj[index]._shadowFrustumSettings);
    }

    unsigned BasicSceneParser::GetLightCount() const 
    { 
        return (unsigned)GetEnvSettings()._lights.size(); 
    }

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

///////////////////////////////////////////////////////////////////////////////////////////////////

    SceneEngine::LightDesc DefaultDominantLight()
    {
        SceneEngine::LightDesc light;
        light._type = SceneEngine::LightDesc::Directional;
        light._negativeLightDirection = Normalize(Float3(-0.15046243f, 0.97377890f, 0.17063323f));
        light._radius = 10000.f;
        light._diffuseColor = Float3(3.2803922f, 2.2372551f, 1.9627452f);
        light._nonMetalSpecularBrightness = 7.5f;
        light._specularColor = Float3(6.7647061f, 6.4117646f, 4.7647061f);
        light._diffuseWideningMax = .9f;
        light._diffuseWideningMin = 0.2f;
        light._diffuseModel = 1;
        return light;
    }

    SceneEngine::GlobalLightingDesc DefaultGlobalLightingDesc()
    {
        SceneEngine::GlobalLightingDesc result;
        result._ambientLight = Float3(0.013921569f, 0.032941177f, 0.042745098f);
        XlCopyString(result._skyTexture, "Game\\xleres\\defaultresources\\sky\\samplesky.dds");
        result._skyReflectionScale = 8.f;
        result._skyReflectionBlurriness = 2.f;
        result._skyBrightness = 0.33f;
        result._doAtmosphereBlur = false;
        return result;
    }

    EnvironmentSettings DefaultEnvironmentSettings()
    {
        EnvironmentSettings result;
        result._globalLightingDesc = DefaultGlobalLightingDesc();

        auto defLight = DefaultDominantLight();
        result._lights.push_back(defLight);

        auto frustumSettings = PlatformRig::DefaultShadowFrustumSettings();
        result._shadowProj.push_back(EnvironmentSettings::ShadowProj { defLight, 0, frustumSettings });

        {
            SceneEngine::LightDesc secondaryLight;
            secondaryLight._type = SceneEngine::LightDesc::Directional;
            secondaryLight._negativeLightDirection = Normalize(Float3(0.71622938f, 0.48972201f, -0.49717990f));
            secondaryLight._radius = 10000.f;
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

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void Fixup(SceneEngine::LightDesc& light, const ParameterBox& props)
    {
        static const auto transformHash = ParameterBox::MakeParameterNameHash("Transform");
        auto transform = Transpose(props.GetParameter(transformHash, Identity<Float4x4>()));
        auto translation = ExtractTranslation(transform);
        light._negativeLightDirection = (MagnitudeSquared(translation) > 1e-5f) ? Normalize(translation) : Float3(0.f, 0.f, 0.f);
    }
    
    namespace EntityTypeName
    {
        static const auto* EnvSettings = (const utf8*)"EnvSettings";
        static const auto* AmbientSettings = (const utf8*)"AmbientSettings";
        static const auto* DirectionalLight = (const utf8*)"DirectionalLight";
        static const auto* ToneMapSettings = (const utf8*)"ToneMapSettings";
        static const auto* ShadowFrustumSettings = (const utf8*)"ShadowFrustumSettings";

        static const auto* OceanLightingSettings = (const utf8*)"OceanLightingSettings";
        static const auto* OceanSettings = (const utf8*)"OceanSettings";
        static const auto* FogVolumeRenderer = (const utf8*)"FogVolumeRenderer";
    }
    
    namespace Attribute
    {
        static const auto AttachedLight = ParameterBox::MakeParameterNameHash("Light");
        static const auto Name = ParameterBox::MakeParameterNameHash("Name");
        static const auto Flags = ParameterBox::MakeParameterNameHash("Flags");
    }

    EnvironmentSettings::EnvironmentSettings(
        InputStreamFormatter<utf8>& formatter, 
        const ::Assets::DirectorySearchRules&)
    {
        using namespace SceneEngine;

        _globalLightingDesc = DefaultGlobalLightingDesc();

        std::vector<std::pair<uint64, DefaultShadowFrustumSettings>> shadowSettings;
        std::vector<uint64> lightNames;
        std::vector<std::pair<uint64, uint64>> lightFrustumLink;    // lightid to shadow settings map

        utf8 buffer[256];

        bool exit = false;
        while (!exit) {
            switch(formatter.PeekNext()) {
            case InputStreamFormatter<utf8>::Blob::BeginElement:
                {
                    InputStreamFormatter<utf8>::InteriorSection name;
                    if (!formatter.TryBeginElement(name)) break;

                    if (!XlComparePrefix(EntityTypeName::AmbientSettings, name._start, name._end - name._start)) {
                        _globalLightingDesc = GlobalLightingDesc(ParameterBox(formatter));
                    } else if (!XlComparePrefix(EntityTypeName::ToneMapSettings, name._start, name._end - name._start)) {
                        AccessorDeserialize(formatter, _toneMapSettings);
                    } else if (!XlComparePrefix(EntityTypeName::DirectionalLight, name._start, name._end - name._start)) {

                        ParameterBox params(formatter);
                        uint64 hashName = 0ull;
                        if (params.GetString(Attribute::Name, buffer, dimof(buffer))) {
                            hashName = Hash64((const char*)buffer);
                        }

                        SceneEngine::LightDesc lightDesc(params);
                        Fixup(lightDesc, params);

                        _lights.push_back(lightDesc);

                        if (params.GetParameter(Attribute::Flags, 0u) & (1<<0)) {
                            lightNames.push_back(hashName);
                        } else {
                            lightNames.push_back(0);    // dummy if shadows are disabled
                        }
                        
                    } else if (!XlComparePrefix(EntityTypeName::ShadowFrustumSettings, name._start, name._end - name._start)) {

                        ParameterBox params(formatter);
                        uint64 hashName = 0ull;
                        if (params.GetString(Attribute::Name, buffer, dimof(buffer))) {
                            hashName = Hash64((const char*)buffer);
                        }

                        shadowSettings.push_back(
                            std::make_pair(hashName, CreateFromParameters<PlatformRig::DefaultShadowFrustumSettings>(params)));

                        uint64 frustumLink = 0;
                        if (params.GetString(Attribute::AttachedLight, buffer, dimof(buffer)))
                            frustumLink = Hash64((const char*)buffer);
                        lightFrustumLink.push_back(std::make_pair(frustumLink, hashName));

                    } else if (!XlComparePrefix(EntityTypeName::OceanLightingSettings, name._start, name._end - name._start)) {
                        _oceanLighting = OceanLightingSettings(ParameterBox(formatter));
                    } else if (!XlComparePrefix(EntityTypeName::OceanSettings, name._start, name._end - name._start)) {
                        _deepOceanSim = DeepOceanSimSettings(ParameterBox(formatter));
                    } else if (!XlComparePrefix(EntityTypeName::FogVolumeRenderer, name._start, name._end - name._start)) {
                        _volFogRenderer = VolumetricFogConfig::Renderer(formatter);
                    } else
                        formatter.SkipElement();
                    
                    formatter.TryEndElement();
                    break;
                }

            case InputStreamFormatter<utf8>::Blob::AttributeName:
                {
                    InputStreamFormatter<utf8>::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    break;
                }

            default:
                exit = true; 
                break;
            }
        }

            // bind shadow settings (mapping via the light name parameter)
        for (unsigned c=0; c<lightFrustumLink.size(); ++c) {
            auto f = std::find_if(shadowSettings.cbegin(), shadowSettings.cend(), 
                [&lightFrustumLink, c](const std::pair<uint64, DefaultShadowFrustumSettings>&i) { return i.first == lightFrustumLink[c].second; });

            auto l = std::find(lightNames.cbegin(), lightNames.cend(), lightFrustumLink[c].first);

            if (f != shadowSettings.end() && l != lightNames.end()) {
                auto lightIndex = std::distance(lightNames.cbegin(), l);
                assert(lightIndex < ptrdiff_t(_lights.size()));

                _shadowProj.push_back(
                    EnvironmentSettings::ShadowProj { _lights[lightIndex], SceneEngine::LightId(lightIndex), f->second });
            }
        }
    }

    EnvironmentSettings::EnvironmentSettings() {}
    EnvironmentSettings::~EnvironmentSettings() {}

    /*std::vector<std::pair<std::string, PlatformRig::EnvironmentSettings>> 
        DeserializeEnvSettings(InputStreamFormatter<utf8>& formatter)
    {
        std::vector<std::pair<std::string, PlatformRig::EnvironmentSettings>> result;
        for (;;) {
            switch(formatter.PeekNext()) {
            case InputStreamFormatter<utf8>::Blob::BeginElement:
                {
                    InputStreamFormatter<utf8>::InteriorSection name;
                    if (!formatter.TryBeginElement(name)) break;
                    auto settings = DeserializeSingleSettings(formatter);
                    if (!formatter.TryEndElement()) break;

                    result.emplace_back(std::move(settings));
                    break;
                }

            default:
                return std::move(result);
            }
        }
    }*/
}
