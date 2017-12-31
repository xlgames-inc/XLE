// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "VegetationSpawn.h"
#include "../Utility/Meta/ClassAccessors.h"
#include "../Utility/Meta/ClassAccessorsImpl.h"
#include "../Utility/Meta/AccessorSerialize.h"

template<> const ClassAccessors& GetAccessors<SceneEngine::VegetationSpawnConfig>()
{
    using Obj = SceneEngine::VegetationSpawnConfig;
    static ClassAccessors props(typeid(Obj).hash_code());
    static bool init = false;
    if (!init) {
        props.Add("BaseGridSpacing",     DefaultGet(Obj, _baseGridSpacing),  DefaultSet(Obj, _baseGridSpacing));
        props.Add("JitterAmount",        DefaultGet(Obj, _jitterAmount),     DefaultSet(Obj, _jitterAmount));
        props.Add("AlignToTerrainUp",    DefaultGet(Obj, _alignToTerrainUp), DefaultSet(Obj, _alignToTerrainUp));

        props.AddChildList<Obj::Material>(
            "Material",
            DefaultCreate(Obj, _materials),
            DefaultGetCount(Obj, _materials),
            DefaultGetChildByIndex(Obj, _materials),
            DefaultGetChildByKey(Obj, _materials));

        props.AddChildList<Obj::ObjectType>(
            "ObjectType",
            DefaultCreate(Obj, _objectTypes),
            DefaultGetCount(Obj, _objectTypes),
            DefaultGetChildByIndex(Obj, _objectTypes),
            DefaultGetChildByKey(Obj, _objectTypes));

        init = true;
    }
    return props;
}

template<> const ClassAccessors& GetAccessors<SceneEngine::VegetationSpawnConfig::ObjectType>()
{
    using Obj = SceneEngine::VegetationSpawnConfig::ObjectType;
    static ClassAccessors props(typeid(Obj).hash_code());
    static bool init = false;
    if (!init) {
        props.Add("Model", DefaultGet(Obj, _modelName), DefaultSet(Obj, _modelName));
        props.Add("Material", DefaultGet(Obj, _materialName), DefaultSet(Obj, _materialName));
        init = true;
    }
    return props;
}

template<> const ClassAccessors& GetAccessors<SceneEngine::VegetationSpawnConfig::Bucket>()
{
    using Obj = SceneEngine::VegetationSpawnConfig::Bucket;
    static ClassAccessors props(typeid(Obj).hash_code());
    static bool init = false;
    if (!init) {
        props.Add("ObjectType", DefaultGet(Obj, _objectType), DefaultSet(Obj, _objectType));
        props.Add("MaxDrawDistance", DefaultGet(Obj, _maxDrawDistance), DefaultSet(Obj, _maxDrawDistance));
        props.Add("FrequencyWeight", DefaultGet(Obj, _frequencyWeight), DefaultSet(Obj, _frequencyWeight));
        init = true;
    }
    return props;
}

template<> const ClassAccessors& GetAccessors<SceneEngine::VegetationSpawnConfig::Material>()
{
    using Obj = SceneEngine::VegetationSpawnConfig::Material;
    static ClassAccessors props(typeid(Obj).hash_code());
    static bool init = false;
    if (!init) {
        props.Add("NoSpawnWeight", DefaultGet(Obj, _noSpawnWeight), DefaultSet(Obj, _noSpawnWeight));
        props.Add("SuppressionThreshold", DefaultGet(Obj, _suppressionThreshold), DefaultSet(Obj, _suppressionThreshold));
        props.Add("SuppressionNoise", DefaultGet(Obj, _suppressionNoise), DefaultSet(Obj, _suppressionNoise));
        props.Add("SuppressionGain", DefaultGet(Obj, _suppressionGain), DefaultSet(Obj, _suppressionGain));
        props.Add("SuppressionLacunarity", DefaultGet(Obj, _suppressionLacunarity), DefaultSet(Obj, _suppressionLacunarity));
        props.Add("MaterialId", DefaultGet(Obj, _materialId), DefaultSet(Obj, _materialId));

        props.AddChildList<SceneEngine::VegetationSpawnConfig::Bucket>(
            "Bucket",
            DefaultCreate(Obj, _buckets),
            DefaultGetCount(Obj, _buckets),
            DefaultGetChildByIndex(Obj, _buckets),
            DefaultGetChildByKey(Obj, _buckets));

        init = true;
    }
    return props;
}

namespace SceneEngine
{

    void VegetationSpawnConfig::Write(OutputStreamFormatter& formatter) const
    {
        AccessorSerialize(formatter, *this);
    }

    VegetationSpawnConfig::VegetationSpawnConfig(
        InputStreamFormatter<utf8>& formatter,
        const ::Assets::DirectorySearchRules& searchRules,
		const ::Assets::DepValPtr& depVal)
    : VegetationSpawnConfig()
    {
        AccessorDeserialize(formatter, *this);
        _searchRules = searchRules;
		_depVal = depVal;
    }

    VegetationSpawnConfig::VegetationSpawnConfig() 
    {
        _baseGridSpacing = 1.f;
        _jitterAmount = 1.f;
        _alignToTerrainUp = false;
    }

    VegetationSpawnConfig::~VegetationSpawnConfig() {}

    VegetationSpawnConfig::Bucket::Bucket()
    {
        _maxDrawDistance = 100.f;
        _frequencyWeight = 1.f;
        _objectType = 0;
    }

    VegetationSpawnConfig::Material::Material()
    {
        _noSpawnWeight = 0.f;
        _suppressionThreshold = -1.f;
        _suppressionNoise = 0.85f * 9.632f;
        _suppressionGain = 1.1f * .85f;
        _suppressionLacunarity = 2.0192f;
        _materialId = 0;
    }

    VegetationSpawnConfig::ObjectType::ObjectType()
    {}

}