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
#include "../../Utility/Conversion.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include <memory>

namespace EntityInterface
{

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
    }
    
    namespace Attribute
    {
        static const auto Flags = ParameterBox::MakeParameterNameHash("Flags");
        static const auto ShadowFrustumSettings = ParameterBox::MakeParameterNameHash("ShadowFrustumSettings");
        static const auto Name = ParameterBox::MakeParameterNameHash("Name");
    }

    PlatformRig::EnvironmentSettings
        BuildEnvironmentSettings(
            const RetainedEntities& flexGobInterface,
            const RetainedEntity& obj)
    {
        using namespace SceneEngine;
        using namespace PlatformRig;

        const auto typeAmbient = flexGobInterface.GetTypeId(EntityTypeName::AmbientSettings);
        const auto typeDirectionalLight = flexGobInterface.GetTypeId(EntityTypeName::DirectionalLight);
        const auto typeToneMapSettings = flexGobInterface.GetTypeId(EntityTypeName::ToneMapSettings);
        const auto shadowFrustumSettings = flexGobInterface.GetTypeId(EntityTypeName::ShadowFrustumSettings);

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
                Fixup(light, props);
                
                if (props.GetParameter(Attribute::Flags, 0u) & (1<<0)) {

                        // look for frustum settings that match the "name" parameter
                    auto frustumSettings = PlatformRig::DefaultShadowFrustumSettings();
                    auto fsRef = props.GetString<char>(Attribute::ShadowFrustumSettings);
                    if (!fsRef.empty()) {
                        for (const auto& cid2 : obj._children) {
                            const auto* fsSetObj = flexGobInterface.GetEntity(obj._doc, cid2);
                            if (!fsSetObj || fsSetObj->_type != shadowFrustumSettings) continue;
                            
                            auto settingsName = fsSetObj->_properties.GetString<char>(Attribute::Name);
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

        const auto typeSettings = flexGobInterface.GetTypeId(EntityTypeName::EnvSettings);
        auto allSettings = flexGobInterface.FindEntitiesOfType(typeSettings);
        for (const auto& s : allSettings)
            result.push_back(
                std::make_pair(
                    s->_properties.GetString<char>(Attribute::Name),
                    BuildEnvironmentSettings(flexGobInterface, *s)));

        return std::move(result);
    }

    template<typename CharType>
        void Serialize(
            OutputStreamFormatter& formatter,
            const RetainedEntity& obj,
            const RetainedEntities& entities)
    {
        // static const auto nameHash = ParameterBox::MakeParameterNameHash((const utf8*)"Name");
        // const auto bufferSize = 256u;
        // StringMeld<bufferSize, CharType> name;
        // if (!obj._properties.GetString(nameHash, const_cast<CharType*>(name.get()), bufferSize))
        //     name << obj._id;

        auto name = Conversion::Convert<std::basic_string<CharType>>(entities.GetTypeName(obj._type));
        auto eleId = formatter.BeginElement(AsPointer(name.cbegin()), AsPointer(name.cend()));
        if (!obj._children.empty()) formatter.NewLine();    // properties can continue on the same line, but only if we don't have children
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

        const auto typeSettings = flexGobInterface.GetTypeId(EntityTypeName::EnvSettings);
        auto allSettings = flexGobInterface.FindEntitiesOfType(typeSettings);

        bool foundAtLeastOne = false;
        for (const auto& s : allSettings)
            if (s->_doc == docId) {
                Serialize<utf8>(formatter, *s, flexGobInterface);
                foundAtLeastOne = true;
            }
        
        if (!foundAtLeastOne)
            ThrowException(::Exceptions::BasicLabel("No environment settings found"));

        formatter.Flush();
    }

    PlatformRig::EnvironmentSettings DeserializeSingleSettings(InputStreamFormatter<utf8>& formatter)
    {
        using namespace SceneEngine;
        using namespace PlatformRig;

        PlatformRig::EnvironmentSettings result;
        result._globalLightingDesc = DefaultGlobalLightingDesc();
        result._toneMapSettings = DefaultToneMapSettings();

        std::vector<std::pair<uint64, DefaultShadowFrustumSettings>> shadowSettings;
        std::vector<uint64> lightFrustumLink;

        utf8 buffer[256];

        bool exit = false;
        while (!exit) {
            switch(formatter.PeekNext()) {
            case InputStreamFormatter<utf8>::Blob::BeginElement:
                {
                    InputStreamFormatter<utf8>::InteriorSection name;
                    if (!formatter.TryReadBeginElement(name)) break;

                    if (!XlComparePrefix(EntityTypeName::AmbientSettings, name._start, name._end - name._start)) {
                        result._globalLightingDesc = GlobalLightingDesc(ParameterBox(formatter));
                    } else if (!XlComparePrefix(EntityTypeName::ToneMapSettings, name._start, name._end - name._start)) {
                        result._toneMapSettings = ToneMapSettings(ParameterBox(formatter));
                    } else if (!XlComparePrefix(EntityTypeName::DirectionalLight, name._start, name._end - name._start)) {

                        ParameterBox params(formatter);
                        SceneEngine::LightDesc lightDesc(params);
                        Fixup(lightDesc, params);

                        result._lights.push_back(lightDesc);

                        uint64 frustumLink = 0;
                        if (params.GetParameter(Attribute::Flags, 0u) & (1<<0)) {
                            if (params.GetString(Attribute::ShadowFrustumSettings, buffer, dimof(buffer)))
                                frustumLink = Hash64((const char*)buffer);
                        }
                        lightFrustumLink.push_back(frustumLink);

                    } else if (!XlComparePrefix(EntityTypeName::ShadowFrustumSettings, name._start, name._end - name._start)) {
                        ParameterBox params(formatter);
                        if (params.GetString(Attribute::Name, buffer, dimof(buffer))) {
                            auto h = Hash64((const char*)buffer);
                            auto i = LowerBound(shadowSettings, h);
                            if (i != shadowSettings.end() && i->first == h) {
                                assert(0); // hash or name conflict
                            } else {
                                shadowSettings.insert(
                                    i, std::make_pair(h, PlatformRig::DefaultShadowFrustumSettings(params)));
                            }
                        }
                    } else
                        formatter.SkipElement();
                    
                    formatter.TryReadEndElement();
                    break;
                }

            case InputStreamFormatter<utf8>::Blob::AttributeName:
                {
                    InputStreamFormatter<utf8>::InteriorSection name, value;
                    formatter.TryReadAttribute(name, value);
                    break;
                }

            default:
                exit = true; 
                break;
            }
        }

        for (unsigned c=0; c<lightFrustumLink.size(); ++c) {
            auto f = LowerBound(shadowSettings, lightFrustumLink[c]);
            if (f != shadowSettings.end() && f->first == lightFrustumLink[c]) {
                result._lights[c]._shadowFrustumIndex = (unsigned)result._shadowProj.size();
                result._shadowProj.push_back(
                    EnvironmentSettings::ShadowProj { result._lights[c], f->second });
            }
        }

        return std::move(result);
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
