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

    PlatformRig::EnvironmentSettings
        BuildEnvironmentSettings(
            const EditorDynamicInterface::FlexObjectType& flexGobInterface,
            const EditorDynamicInterface::FlexObjectType::Object& obj)
    {
        using namespace SceneEngine;
        using namespace PlatformRig;

        const auto typeAmbient = flexGobInterface.GetTypeId("AmbientSettings");
        const auto typeDirectionalLight = flexGobInterface.GetTypeId("DirectionalLight");
        const auto typeToneMapSettings = flexGobInterface.GetTypeId("ToneMapSettings");
        const auto shadowFrustumSettings = flexGobInterface.GetTypeId("ShadowFrustumSettings");

        EnvironmentSettings result;
        result._globalLightingDesc = DefaultGlobalLightingDesc();
        result._toneMapSettings = DefaultToneMapSettings();
        for (auto cid : obj._children) {
            const auto* child = flexGobInterface.GetObject(obj._doc, cid);
            if (!child) continue;

            if (child->_type == typeAmbient) {
                result._globalLightingDesc = GlobalLightingDesc(child->_properties);
            }

            if (child->_type == typeDirectionalLight) {
                const auto& props = child->_properties;

                LightDesc light(props);
                static const auto flagsHash = ParameterBox::MakeParameterNameHash("flags");
                static const auto transformHash = ParameterBox::MakeParameterNameHash("Transform");
                static const auto shadowFrustumSettingsHash = ParameterBox::MakeParameterNameHash("ShadowFrustumSettings");
                
                auto transform = Transpose(props.GetParameter(transformHash, Identity<Float4x4>()));
                auto translation = ExtractTranslation(transform);
                light._negativeLightDirection = (MagnitudeSquared(translation) > 0.01f) ? Normalize(translation) : Float3(0.f, 0.f, 0.f);

                if (props.GetParameter(flagsHash, 0u) & (1<<0)) {

                        // look for frustum settings that match the "name" parameter
                    auto frustumSettings = PlatformRig::DefaultShadowFrustumSettings();
                    auto fsRef = props.GetString<char>(shadowFrustumSettingsHash);
                    if (!fsRef.empty()) {
                        for (auto cid2 : obj._children) {
                            const auto* fsSetObj = flexGobInterface.GetObject(obj._doc, cid2);
                            if (!fsSetObj || fsSetObj->_type != shadowFrustumSettings) continue;
                            
                            static const auto nameHash = ParameterBox::MakeParameterNameHash("Name");
                            auto settingsName = fsSetObj->_properties.GetString<char>(nameHash);
                            if (XlCompareStringI(settingsName.c_str(), fsRef.c_str())!=0) continue;

                            frustumSettings = PlatformRig::DefaultShadowFrustumSettings(fsSetObj->_properties);
                            break;
                        }
                    }

                    light._shadowFrustumIndex = (unsigned)result._shadowProj.size();
                    result._shadowProj.push_back(
                        EnvironmentSettings::ShadowProj { light, frustumSettings });
                }

                result._lights.push_back(light);
            }

            if (child->_type == typeToneMapSettings) {
                result._toneMapSettings = ToneMapSettings(child->_properties);
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
