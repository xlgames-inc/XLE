// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EnvironmentSettings.h"
#include "LevelEditorScene.h"
#include "MarshalString.h"
#include "../../PlatformRig/BasicSceneParser.h"
#include "../../Math/Transformations.h"

namespace GUILayer
{

///////////////////////////////////////////////////////////////////////////////////////////////////

   static Float3 AsFloat3Color(unsigned packedColor)
    {
        return Float3(
            (float)((packedColor >> 16) & 0xff) / 255.f,
            (float)((packedColor >>  8) & 0xff) / 255.f,
            (float)(packedColor & 0xff) / 255.f);
    }

    PlatformRig::EnvironmentSettings
        BuildEnvironmentSettings(
            const EditorDynamicInterface::FlexObjectType& flexGobInterface,
            const EditorDynamicInterface::FlexObjectType::Object& obj)
    {
        using namespace SceneEngine;
        using namespace PlatformRig;

        const auto typeAmbient = flexGobInterface.GetTypeId("AmbientSettings");
        const auto typeDirectionalLight = flexGobInterface.GetTypeId("DirectionalLight");

        EnvironmentSettings result;
        for (auto cid : obj._children) {
            const auto* child = flexGobInterface.GetObject(obj._doc, cid);
            if (!child) continue;

            if (child->_type == typeAmbient) {
                result._globalLightingDesc = GlobalLightingDesc(child->_properties);
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
                    light._shadowFrustumIndex = (unsigned)result._shadowProj.size();
                    result._shadowProj.push_back(
                        EnvironmentSettings::ShadowProj { light, PlatformRig::DefaultShadowFrustumSettings() });
                }

                result._lights.push_back(light);
            }
        }

        return std::move(result);
    }

    EnvSettingsVector BuildEnvironmentSettings(
        const EditorDynamicInterface::FlexObjectType& flexGobInterface)
    {
        EnvSettingsVector result;

        const auto typeSettings = flexGobInterface.GetTypeId("EnvSettings");
        static const auto nameHash = ParameterBox::MakeParameterNameHash("name");
        auto allSettings = flexGobInterface.FindObjectsOfType(typeSettings);
        for (auto s : allSettings)
            result.push_back(std::make_pair(
                s->_properties.GetString<char>(nameHash),
                BuildEnvironmentSettings(flexGobInterface, *s)));

        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    IEnumerable<String^>^ EnvironmentSettingsSet::Names::get()
    {
        auto result = gcnew List<String^>();
        for (auto i=_settings->cbegin(); i!=_settings->cend(); ++i)
            result->Add(clix::marshalString<clix::E_UTF8>(i->first));
        return result;
    }

    void EnvironmentSettingsSet::AddDefault()
    {
        _settings->push_back(
            std::make_pair(std::string("Default"), PlatformRig::DefaultEnvironmentSettings()));
    }

    const PlatformRig::EnvironmentSettings& EnvironmentSettingsSet::GetSettings(String^ name)
    {
        auto nativeName = clix::marshalString<clix::E_UTF8>(name);
        for (auto i=_settings->cbegin(); i!=_settings->cend(); ++i)
            if (!XlCompareStringI(nativeName.c_str(), i->first.c_str()))
                return i->second;
        if (!_settings->empty()) return (*_settings)[0].second;

        return *(const PlatformRig::EnvironmentSettings*)nullptr;
    }

    EnvironmentSettingsSet::EnvironmentSettingsSet(EditorSceneManager^ scene)
    {
        _settings.reset(new EnvSettingsVector());
        *_settings = BuildEnvironmentSettings(scene->GetFlexObjects());
    }

    EnvironmentSettingsSet::~EnvironmentSettingsSet() {}

}
