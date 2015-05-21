// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4793) // 'Exceptions::BasicLabel::BasicLabel' : function compiled as native :

#include "EnvironmentSettings.h"
#include "../../PlatformRig/BasicSceneParser.h"
#include "../../Math/Transformations.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/Streams/Data.h"
#include <memory>

namespace GUILayer
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    PlatformRig::EnvironmentSettings
        BuildEnvironmentSettings(
            const EditorDynamicInterface::FlexObjectScene& flexGobInterface,
            const EditorDynamicInterface::FlexObjectScene::Object& obj)
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
        for (const auto& cid : obj._children) {
            const auto* child = flexGobInterface.GetObject(obj._doc, cid);
            if (!child) continue;

            if (child->_type == typeAmbient) {
                result._globalLightingDesc = GlobalLightingDesc(child->_properties);
            }

            if (child->_type == typeDirectionalLight) {
                const auto& props = child->_properties;

                LightDesc light(props);
                static const auto flagsHash = ParameterBox::MakeParameterNameHash("Flags");
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
                        for (const auto& cid2 : obj._children) {
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
        const EditorDynamicInterface::FlexObjectScene& flexGobInterface)
    {
        EnvSettingsVector result;

        const auto typeSettings = flexGobInterface.GetTypeId("EnvSettings");
        static const auto nameHash = ParameterBox::MakeParameterNameHash("name");
        auto allSettings = flexGobInterface.FindObjectsOfType(typeSettings);
        for (const auto& s : allSettings)
            result.push_back(
                std::make_pair(
                    s->_properties.GetString<char>(nameHash),
                    BuildEnvironmentSettings(flexGobInterface, *s)));

        return std::move(result);
    }

    std::unique_ptr<Utility::Data> BuildData(
        const EditorDynamicInterface::FlexObjectScene::Object& obj,
        const EditorDynamicInterface::FlexObjectScene& flexGobInterface)
    {
        assert(0);
        return std::make_unique<Utility::Data>();
    }

    void ExportEnvSettings(
        const EditorDynamicInterface::FlexObjectScene& flexGobInterface,
        EditorDynamicInterface::DocumentId docId,
        const ::Assets::ResChar destinationFile[])
    {
            // Save out the environment settings in the given document
            // in native format.
            // Our environment settings are stored in the flex object interface,
            // so we first need to convert them into native format, and then 
            // we'll write those settings to a basic text file.
            // There are two ways to handle this:
            //      1) convert from the flex gob objects into PlatformRig::EnvironmentSettings
            //          and then serialise from there
            //      2) just get the parameter boxes in the flex gob objects, and write
            //          them out
            // The second way is a lot easier. And it's much more straight-forward to
            // maintain.

        const auto typeSettings = flexGobInterface.GetTypeId("EnvSettings");
        auto allSettings = flexGobInterface.FindObjectsOfType(typeSettings);

        auto asData = std::make_unique<Utility::Data>();

        bool foundAtLeastOne = false;
        for (const auto& s : allSettings)
            if (s->_doc == docId)
            {
                asData->Add(BuildData(*s, flexGobInterface).release());
                foundAtLeastOne = true;
            }

        
        if (!foundAtLeastOne)
            ThrowException(::Exceptions::BasicLabel("No environment settings found"));

        auto saveRes = asData->Save(destinationFile);

        if (!saveRes)
            ThrowException(::Exceptions::BasicLabel("Error while serializing environment settings"));
    }

}
