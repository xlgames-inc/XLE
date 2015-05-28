// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4793) // 'Exceptions::BasicLabel::BasicLabel' : function compiled as native :

#include "EnvironmentSettings.h"
#include "RetainedEntities.h"
#include "../../PlatformRig/BasicSceneParser.h"
#include "../../Math/Transformations.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include <memory>

namespace EntityInterface
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    PlatformRig::EnvironmentSettings
        BuildEnvironmentSettings(
            const RetainedEntities& flexGobInterface,
            const RetainedEntity& obj)
    {
        using namespace SceneEngine;
        using namespace PlatformRig;

        const auto typeAmbient = flexGobInterface.GetTypeId((const utf8*)"AmbientSettings");
        const auto typeDirectionalLight = flexGobInterface.GetTypeId((const utf8*)"DirectionalLight");
        const auto typeToneMapSettings = flexGobInterface.GetTypeId((const utf8*)"ToneMapSettings");
        const auto shadowFrustumSettings = flexGobInterface.GetTypeId((const utf8*)"ShadowFrustumSettings");

        EnvironmentSettings result;
        result._globalLightingDesc = DefaultGlobalLightingDesc();
        result._toneMapSettings = DefaultToneMapSettings();
        for (const auto& cid : obj._children) {
            const auto* child = flexGobInterface.GetEntity(obj._doc, cid);
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
                            const auto* fsSetObj = flexGobInterface.GetEntity(obj._doc, cid2);
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
        const RetainedEntities& flexGobInterface)
    {
        EnvSettingsVector result;

        const auto typeSettings = flexGobInterface.GetTypeId((const utf8*)"EnvSettings");
        static const auto nameHash = ParameterBox::MakeParameterNameHash((const utf8*)"Name");
        auto allSettings = flexGobInterface.FindEntitiesOfType(typeSettings);
        for (const auto& s : allSettings)
            result.push_back(
                std::make_pair(
                    s->_properties.GetString<char>(nameHash),
                    BuildEnvironmentSettings(flexGobInterface, *s)));

        return std::move(result);
    }

    template<typename CharType>
        void Serialize(
            OutputStreamFormatter& formatter,
            const RetainedEntity& obj,
            const RetainedEntities& entities)
    {
        static const auto nameHash = ParameterBox::MakeParameterNameHash((const utf8*)"Name");
        const auto bufferSize = 256u;
        StringMeld<bufferSize, CharType> name;
        if (!obj._properties.GetString(nameHash, const_cast<CharType*>(name.get()), bufferSize))
            name << obj._id;

        auto eleId = formatter.BeginElement(name);
        obj._properties.Serialize<CharType>(formatter);

        for (auto c=obj._children.cbegin(); c!=obj._children.cend(); ++c) {
            const auto* child = entities.GetEntity(obj._doc, *c);
            if (child)
                Serialize<CharType>(formatter, *child, entities);
        }

        formatter.EndElement(eleId);
    }

    void ExportEnvSettings(
        OutputStreamFormatter& formatter,
        const RetainedEntities& flexGobInterface,
        DocumentId docId)
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

        const auto typeSettings = flexGobInterface.GetTypeId((const utf8*)"EnvSettings");
        auto allSettings = flexGobInterface.FindEntitiesOfType(typeSettings);

        bool foundAtLeastOne = false;
        for (const auto& s : allSettings)
            if (s->_doc == docId) {
                Serialize<utf8>(formatter, *s, flexGobInterface);
                foundAtLeastOne = true;
            }
        
        if (!foundAtLeastOne)
            ThrowException(::Exceptions::BasicLabel("No environment settings found"));
    }

    PlatformRig::EnvironmentSettings DeserializeSingleSettings(InputStreamFormatter<utf8>& formatter)
    {
        PlatformRig::EnvironmentSettings result;
        for (;;) {
            switch(formatter.PeekNext()) {
            case InputStreamFormatter<utf8>::Blob::BeginElement:
                {
                    InputStreamFormatter<utf8>::InteriorSection name;
                    if (!formatter.TryReadBeginElement(name)) break;
                    ParameterBox params(formatter);
                    if (!formatter.TryReadEndElement()) break;

                    result._lights.push_back(SceneEngine::LightDesc(params));
                    break;
                }

            case InputStreamFormatter<utf8>::Blob::AttributeName:
                {
                    InputStreamFormatter<utf8>::InteriorSection name, value;
                    formatter.TryReadAttribute(name, value);
                    break;
                }

            default:
                return std::move(result);
            }
        }
    }

    EnvSettingsVector DeserializeEnvSettings(InputStreamFormatter<utf8>& formatter)
    {
        EnvSettingsVector result;

        for (;;) {
            switch(formatter.PeekNext()) {
            case InputStreamFormatter<utf8>::Blob::BeginElement:
                {
                    InputStreamFormatter<utf8>::InteriorSection name;
                    if (!formatter.TryReadBeginElement(name)) break;
                    auto settings = DeserializeSingleSettings(formatter);
                    if (!formatter.TryReadEndElement()) break;

                    result.emplace_back(
                        std::make_pair(
                            std::string((const char*)name._start, (const char*)name._end), 
                            std::move(settings)));
                    break;
                }

            default:
                return std::move(result);
            }
        }
    }

}
