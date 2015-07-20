// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4793) // 'Exceptions::BasicLabel::BasicLabel' : function compiled as native :

#include "EnvironmentSettings.h"
#include "RetainedEntities.h"
#include "../../SceneEngine/Ocean.h"
#include "../../SceneEngine/VolumetricFog.h"
#include "../../SceneEngine/ShallowSurface.h"
#include "../../PlatformRig/BasicSceneParser.h"
#include "../../Math/Transformations.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/Conversion.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include <memory>

namespace SceneEngine 
{
    extern OceanSettings GlobalOceanSettings; 
    extern OceanLightingSettings GlobalOceanLightingSettings;
}

namespace EntityInterface
{
    #define ParamName(x) static auto x = ParameterBox::MakeParameterNameHash(#x);

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
                    if (!formatter.TryBeginElement(name)) break;

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
                    if (!formatter.TryBeginElement(name)) break;
                    auto settings = DeserializeSingleSettings(formatter);
                    if (!formatter.TryEndElement()) break;

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


///////////////////////////////////////////////////////////////////////////////////////////////////

    static Float3 AsFloat3Color(unsigned packedColor)
    {
        return Float3(
            (float)((packedColor >> 16) & 0xff) / 255.f,
            (float)((packedColor >>  8) & 0xff) / 255.f,
            (float)(packedColor & 0xff) / 255.f);
    }

    static SceneEngine::OceanSettings BuildOceanSettings(const RetainedEntities& sys, const RetainedEntity& obj)
    {
        ParamName(Enable);
        ParamName(WindAngle);
        ParamName(WindVelocity);
        ParamName(PhysicalDimensions);
        ParamName(GridDimensions);
        ParamName(StrengthConstantXY);
        ParamName(StrengthConstantZ);
        ParamName(DetailNormalsStrength);
        ParamName(SpectrumFade);
        ParamName(ScaleAgainstWind);
        ParamName(SuppressionFactor);
        ParamName(GridShiftSpeed);
        ParamName(BaseHeight);
        ParamName(FoamThreshold);
        ParamName(FoamIncreaseSpeed);
        ParamName(FoamIncreaseClamp);
        ParamName(FoamDecrease);

        SceneEngine::OceanSettings result;
        result._enable = obj._properties.GetParameter(Enable, result._enable);
        result._windAngle[0] = obj._properties.GetParameter(WindAngle, result._windAngle[0] * (180.f / gPI)) * (gPI / 180.f);
        result._windVelocity[0] = obj._properties.GetParameter(WindVelocity, result._windVelocity[0]);
        result._physicalDimensions = obj._properties.GetParameter(PhysicalDimensions, result._physicalDimensions);
        result._gridDimensions = obj._properties.GetParameter(GridDimensions, result._gridDimensions);
        result._strengthConstantXY = obj._properties.GetParameter(StrengthConstantXY, result._strengthConstantXY);
        result._strengthConstantZ = obj._properties.GetParameter(StrengthConstantZ, result._strengthConstantZ);
        result._detailNormalsStrength = obj._properties.GetParameter(DetailNormalsStrength, result._detailNormalsStrength);
        result._spectrumFade = obj._properties.GetParameter(SpectrumFade, result._spectrumFade);
        result._scaleAgainstWind[0] = obj._properties.GetParameter(ScaleAgainstWind, result._scaleAgainstWind[0]);
        result._suppressionFactor[0] = obj._properties.GetParameter(SuppressionFactor, result._suppressionFactor[0]);
        result._gridShiftSpeed = obj._properties.GetParameter(GridShiftSpeed, result._gridShiftSpeed);
        result._baseHeight = obj._properties.GetParameter(BaseHeight, result._baseHeight);
        result._foamThreshold = obj._properties.GetParameter(FoamThreshold, result._foamThreshold);
        result._foamIncreaseSpeed = obj._properties.GetParameter(FoamIncreaseSpeed, result._foamIncreaseSpeed);
        result._foamIncreaseClamp = obj._properties.GetParameter(FoamIncreaseClamp, result._foamIncreaseClamp);
        result._foamDecrease = obj._properties.GetParameter(FoamDecrease, result._foamDecrease);
        return result;
    }

    static SceneEngine::OceanLightingSettings BuildOceanLightingSettings(const RetainedEntities& sys, const RetainedEntity& obj)
    {
        ParamName(SpecularReflectionBrightness);
        ParamName(FoamBrightness);
        ParamName(OpticalThicknessScalar);
        ParamName(OpticalThicknessColor);
        ParamName(SkyReflectionBrightness);
        ParamName(SpecularPower);
        ParamName(UpwellingScale);
        ParamName(RefractiveIndex);
        ParamName(ReflectionBumpScale);
        ParamName(DetailNormalFrequency);
        ParamName(SpecularityFrequency);
        ParamName(MatSpecularMin);
        ParamName(MatSpecularMax);
        ParamName(MatRoughness);

        SceneEngine::OceanLightingSettings result;
        result._specularReflectionBrightness = obj._properties.GetParameter(SpecularReflectionBrightness, result._specularReflectionBrightness);
        result._foamBrightness = obj._properties.GetParameter(FoamBrightness, result._foamBrightness);

        auto otColor = obj._properties.GetParameter<unsigned>(OpticalThicknessColor);
        auto otScalar = obj._properties.GetParameter<float>(OpticalThicknessScalar);
        if (otColor.first && otScalar.first) {
            result._opticalThickness = AsFloat3Color(otColor.second) * otScalar.second;
        }

        result._skyReflectionBrightness = obj._properties.GetParameter(SkyReflectionBrightness, result._skyReflectionBrightness);
        result._specularPower = obj._properties.GetParameter(SpecularPower, result._specularPower);
        result._upwellingScale = obj._properties.GetParameter(UpwellingScale, result._upwellingScale);
        result._refractiveIndex = obj._properties.GetParameter(RefractiveIndex, result._refractiveIndex);
        result._reflectionBumpScale = obj._properties.GetParameter(ReflectionBumpScale, result._reflectionBumpScale);
        result._detailNormalFrequency = obj._properties.GetParameter(DetailNormalFrequency, result._detailNormalFrequency);
        result._specularityFrequency = obj._properties.GetParameter(SpecularityFrequency, result._specularityFrequency);
        result._matSpecularMin = obj._properties.GetParameter(MatSpecularMin, result._matSpecularMin);
        result._matSpecularMax = obj._properties.GetParameter(MatSpecularMax, result._matSpecularMax);
        result._matRoughness = obj._properties.GetParameter(MatRoughness, result._matRoughness);
        return result;
    }

    void RegisterEnvironmentFlexObjects(RetainedEntities& flexSys)
    {
        flexSys.RegisterCallback(
            flexSys.GetTypeId((const utf8*)"OceanSettings"),
            [](const RetainedEntities& flexSys, const Identifier& obj)
            {
                auto* object = flexSys.GetEntity(obj);
                if (object)
                    SceneEngine::GlobalOceanSettings = BuildOceanSettings(flexSys, *object);
            }
        );

        flexSys.RegisterCallback(
            flexSys.GetTypeId((const utf8*)"OceanLightingSettings"),
            [](const RetainedEntities& flexSys, const Identifier& obj)
            {
                auto* object = flexSys.GetEntity(obj);
                if (object)
                    SceneEngine::GlobalOceanLightingSettings = BuildOceanLightingSettings(flexSys, *object);
            }
        );
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void UpdateVolumetricFog(
        const RetainedEntities& sys, const RetainedEntity& obj,
        SceneEngine::VolumetricFogManager& mgr)
    {
        using namespace SceneEngine;
        VolumetricFogConfig cfg;

        const auto volumeType = sys.GetTypeId((const utf8*)"FogVolume");
        auto volumes = sys.FindEntitiesOfType(volumeType);
        for (auto v:volumes) {
            const auto& props = v->_properties;
            cfg._volumes.push_back(VolumetricFogConfig::FogVolume(props));
        }

        const auto rendererConfigType = sys.GetTypeId((const utf8*)"FogVolumeRenderer");
        auto renderers = sys.FindEntitiesOfType(rendererConfigType);
        if (!renderers.empty())
            cfg._renderer = VolumetricFogConfig::Renderer(renderers[0]->_properties);
        
        mgr.Load(cfg);
    }

    void RegisterVolumetricFogFlexObjects(
        RetainedEntities& flexSys, 
        std::shared_ptr<SceneEngine::VolumetricFogManager> manager)
    {
        std::weak_ptr<SceneEngine::VolumetricFogManager> weakPtrToManager = manager;
        const utf8* types[] = { (const utf8*)"FogVolume", (const utf8*)"FogVolumeRenderer" };
        for (unsigned c=0; c<dimof(types); ++c) {
            flexSys.RegisterCallback(
                flexSys.GetTypeId(types[c]),
                [weakPtrToManager](const RetainedEntities& flexSys, const Identifier& obj)
                {
                    auto mgr = weakPtrToManager.lock();
                    if (!mgr) return;

                    auto* object = flexSys.GetEntity(obj);
                    if (object)
                        UpdateVolumetricFog(flexSys, *object, *mgr);
                }
            );
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static Float4x4 GetTransform(const RetainedEntity& obj)
    {
        ParamName(Transform);
        ParamName(Translation);

        auto xform = obj._properties.GetParameter<Float4x4>(Transform);
        if (xform.first) return Transpose(xform.second);

        auto transl = obj._properties.GetParameter<Float3>(Translation);
        if (transl.first) {
            return AsFloat4x4(transl.second);
        }
        return Identity<Float4x4>();
    }

    std::vector<Float2> ExtractVertices(const RetainedEntities& sys, const RetainedEntity& obj)
    {
            // Find the vertices within a tri mesh marker type entity,
            // and return as a triangle list of 2d positions.
        ParamName(IndexList);

        auto indexListType = obj._properties.GetParameterType(IndexList);
        if (indexListType._type == ImpliedTyping::TypeCat::Void || indexListType._arrayCount < 3)
            return std::vector<Float2>();

        auto ibData = std::make_unique<unsigned[]>(indexListType._arrayCount);
        bool success = obj._properties.GetParameter(
            IndexList, ibData.get(), 
            ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::UInt32, indexListType._arrayCount));
        if (!success) std::vector<Float2>();

        const auto& chld = obj._children;
        if (!chld.size()) std::vector<Float2>();

        auto vbData = std::make_unique<Float3[]>(chld.size());
        for (size_t c=0; c<chld.size(); ++c) {
            const auto* e = sys.GetEntity(obj._doc, chld[c]);
            if (e) {
                vbData[c] = ExtractTranslation(GetTransform(*e));
            } else {
                vbData[c] = Zero<Float3>();
            }
        }

        std::vector<Float2> result;
        result.reserve(indexListType._arrayCount);
        for (unsigned c=0; c<indexListType._arrayCount; ++c) {
            auto i = ibData[c];
            result.push_back(Truncate(vbData[i]));
        }

        return std::move(result);
    }

    static void UpdateShallowSurface(
        const RetainedEntities& sys, const RetainedEntity& obj,
        SceneEngine::ShallowSurfaceManager& mgr)
    {
        using namespace SceneEngine;

        ParamName(Marker);
        ParamName(Name);

        mgr.Clear();

        const auto surfaceType = sys.GetTypeId((const utf8*)"ShallowSurface");
        const auto markerType = sys.GetTypeId((const utf8*)"TriMeshMarker");

            // Create new surface objects for all of the "ShallowSurface" objects
        auto surfaces = sys.FindEntitiesOfType(surfaceType);
        auto markers = sys.FindEntitiesOfType(markerType);
        for (auto s:surfaces) {
            const auto& props = s->_properties;
            auto markerName = props.GetString<utf8>(Marker);
            if (markerName.empty()) continue;

                // Look for the marker with the matching name
            const RetainedEntity* marker = nullptr;
            for (auto m:markers) {
                auto testName = m->_properties.GetString<utf8>(Name);
                if (XlEqStringI(testName, markerName)) {
                    marker = m;
                    break;
                }
            }

            if (marker) {
                auto verts = ExtractVertices(sys, *marker);
                if (verts.empty()) continue;

                ShallowSurface::Config cfg { 2.f, 32 };
                mgr.Add(
                    std::make_shared<ShallowSurface>(
                        AsPointer(verts.cbegin()), sizeof(Float2), 
                        verts.size(), cfg));
            }
        }
    }

    void RegisterShallowSurfaceFlexObjects(
        RetainedEntities& flexSys, 
        std::shared_ptr<SceneEngine::ShallowSurfaceManager> manager)
    {
        std::weak_ptr<SceneEngine::ShallowSurfaceManager> weakPtrToManager = manager;
        const utf8* types[] = { (const utf8*)"ShallowSurface" };
        for (unsigned c=0; c<dimof(types); ++c) {
            flexSys.RegisterCallback(
                flexSys.GetTypeId(types[c]),
                [weakPtrToManager](const RetainedEntities& flexSys, const Identifier& obj)
                {
                    auto mgr = weakPtrToManager.lock();
                    if (!mgr) return;

                    auto* object = flexSys.GetEntity(obj);
                    if (object)
                        UpdateShallowSurface(flexSys, *object, *mgr);
                }
            );
        }
    }


}
