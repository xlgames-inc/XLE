// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4793) // 'Exceptions::BasicLabel::BasicLabel' : function compiled as native :

#include "EnvironmentSettings.h"
#include "RetainedEntities.h"
#include "../../SceneEngine/Ocean.h"
#include "../../SceneEngine/DeepOceanSim.h"
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
    extern DeepOceanSimSettings GlobalOceanSettings; 
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

        static const auto* OceanLightingSettings = (const utf8*)"OceanLightingSettings";
        static const auto* OceanSettings = (const utf8*)"OceanSettings";
        static const auto* FogVolumeRenderer = (const utf8*)"FogVolumeRenderer";
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

    EnvSettingsVector BuildEnvironmentSettings(const RetainedEntities& flexGobInterface)
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
        void SerializeBody(
            OutputStreamFormatter& formatter,
            const RetainedEntity& obj,
            const RetainedEntities& entities)
    {
        obj._properties.Serialize<CharType>(formatter);

        for (auto c=obj._children.cbegin(); c!=obj._children.cend(); ++c) {
            const auto* child = entities.GetEntity(obj._doc, *c);
            if (child)
                Serialize<CharType>(formatter, *child, entities);
        }
    }

    template<typename CharType>
        void Serialize(
            OutputStreamFormatter& formatter,
            const RetainedEntity& obj,
            const RetainedEntities& entities)
    {
        auto name = Conversion::Convert<std::basic_string<CharType>>(entities.GetTypeName(obj._type));
        auto eleId = formatter.BeginElement(AsPointer(name.cbegin()), AsPointer(name.cend()));
        if (!obj._children.empty()) formatter.NewLine();    // properties can continue on the same line, but only if we don't have children
        SerializeBody<CharType>(formatter, obj, entities);
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
                {
                    auto name = s->_properties.GetString<utf8>(Attribute::Name);
                    auto eleId = formatter.BeginElement(AsPointer(name.cbegin()), AsPointer(name.cend()));
                    formatter.NewLine();
                    SerializeBody<utf8>(formatter, *s, flexGobInterface);
                    formatter.EndElement(eleId);
                }

                foundAtLeastOne = true;
            }
        
        if (!foundAtLeastOne)
            Throw(::Exceptions::BasicLabel("No environment settings found"));

        formatter.Flush();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static SceneEngine::DeepOceanSimSettings BuildOceanSettings(
        const RetainedEntities& sys, const RetainedEntity& obj)
    {
        return SceneEngine::DeepOceanSimSettings(obj._properties);
    }

    static SceneEngine::OceanLightingSettings BuildOceanLightingSettings(
        const RetainedEntities& sys, const RetainedEntity& obj)
    {
        return SceneEngine::OceanLightingSettings(obj._properties);
    }

    void EnvEntitiesManager::RegisterEnvironmentFlexObjects()
    {
        _flexSys->RegisterCallback(
            _flexSys->GetTypeId((const utf8*)"OceanSettings"),
            [](const RetainedEntities& flexSys, const Identifier& obj, RetainedEntities::ChangeType changeType)
            {
                if (changeType != RetainedEntities::ChangeType::Delete) {
                    auto* object = flexSys.GetEntity(obj);
                    if (object)
                        SceneEngine::GlobalOceanSettings = BuildOceanSettings(flexSys, *object);
                } else {
                    SceneEngine::GlobalOceanSettings._enable = false;
                }
            });

        _flexSys->RegisterCallback(
            _flexSys->GetTypeId((const utf8*)"OceanLightingSettings"),
            [](const RetainedEntities& flexSys, const Identifier& obj, RetainedEntities::ChangeType changeType)
            {
                if (changeType != RetainedEntities::ChangeType::Delete) {
                    auto* object = flexSys.GetEntity(obj);
                    if (object)
                        SceneEngine::GlobalOceanLightingSettings = BuildOceanLightingSettings(flexSys, *object);
                } else {
                    SceneEngine::GlobalOceanLightingSettings = SceneEngine::OceanLightingSettings();
                }
            });
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void UpdateVolumetricFog(
        const RetainedEntities& sys, SceneEngine::VolumetricFogManager& mgr)
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

    void EnvEntitiesManager::RegisterVolumetricFogFlexObjects(
        std::shared_ptr<SceneEngine::VolumetricFogManager> manager)
    {
        std::weak_ptr<SceneEngine::VolumetricFogManager> weakPtrToManager = manager;
        const utf8* types[] = { (const utf8*)"FogVolume", (const utf8*)"FogVolumeRenderer" };
        for (unsigned c=0; c<dimof(types); ++c) {
            _flexSys->RegisterCallback(
                _flexSys->GetTypeId(types[c]),
                [weakPtrToManager](const RetainedEntities& flexSys, const Identifier&, RetainedEntities::ChangeType)
                {
                    auto mgr = weakPtrToManager.lock();
                    if (!mgr) return;
                    UpdateVolumetricFog(flexSys, *mgr);
                });
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
        const RetainedEntities& sys,
        SceneEngine::ShallowSurfaceManager& mgr)
    {
        using namespace SceneEngine;

        ParamName(Marker);
        ParamName(name);
        ParamName(GridPhysicalSize)
        ParamName(GridDims)
        ParamName(SimGridCount)
        ParamName(BaseHeight)
        ParamName(SimMethod)
        ParamName(RainQuantity)
        ParamName(EvaporationConstant)
        ParamName(PressureConstant)

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
                auto testName = m->_properties.GetString<utf8>(name);
                if (XlEqStringI(testName, markerName)) {
                    marker = m;
                    break;
                }
            }

            if (marker) {
                    // We have to be careful here -- because we get update
                    // callbacks when the markers change, the marker might
                    // only be partially constructed. Sometimes the vertex
                    // positions aren't properly set -- which causes us to
                    // generate bad triangles.
                auto verts = ExtractVertices(sys, *marker);
                if (verts.empty()) continue;

                ShallowSurface::Config cfg;
                cfg._gridPhysicalSize = s->_properties.GetParameter(GridPhysicalSize, 64.f);
                cfg._simGridDims = s->_properties.GetParameter(GridDims, 128);
                cfg._simGridCount = s->_properties.GetParameter(SimGridCount, 12);
                cfg._baseHeight = s->_properties.GetParameter(BaseHeight, 0.f);
                auto simMethod = s->_properties.GetParameter(SimMethod, 0);
                cfg._usePipeModel = (simMethod==0);
                cfg._rainQuantity = s->_properties.GetParameter(RainQuantity, 0.f);
                cfg._evaporationConstant = s->_properties.GetParameter(EvaporationConstant, 1.f);
                cfg._pressureConstant = s->_properties.GetParameter(PressureConstant, 150.f);

                mgr.Add(
                    std::make_shared<ShallowSurface>(
                        AsPointer(verts.cbegin()), sizeof(Float2), 
                        verts.size(), cfg));
            }
        }
    }

    void EnvEntitiesManager::RegisterShallowSurfaceFlexObjects(
        std::shared_ptr<SceneEngine::ShallowSurfaceManager> manager)
    {
        _shallowWaterManager = manager;

        std::weak_ptr<EnvEntitiesManager> weakPtrToThis = shared_from_this();
        const utf8* types[] = { (const utf8*)"ShallowSurface", (const utf8*)"TriMeshMarker" };
        for (unsigned c=0; c<dimof(types); ++c) {
            _flexSys->RegisterCallback(
                _flexSys->GetTypeId(types[c]),
                [weakPtrToThis](const RetainedEntities&, const Identifier&, RetainedEntities::ChangeType)
                {
                    auto mgr = weakPtrToThis.lock();
                    if (!mgr) return;
                    mgr->_pendingShallowSurfaceUpdate = true;
                });
        }
    }

    void EnvEntitiesManager::FlushUpdates()
    {
        if (_pendingShallowSurfaceUpdate) {
            auto mgr = _shallowWaterManager.lock();
            if (mgr) {
                UpdateShallowSurface(*_flexSys, *mgr);
            }
            _pendingShallowSurfaceUpdate = false;
        }
    }

    EnvEntitiesManager::EnvEntitiesManager(std::shared_ptr<RetainedEntities> sys)
    : _flexSys(std::move(sys))
    , _pendingShallowSurfaceUpdate(false)
    {}

    EnvEntitiesManager::~EnvEntitiesManager() {}


}
