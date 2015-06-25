// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RetainedEntities.h"
#include "../../SceneEngine/VegetationSpawn.h"

namespace EntityInterface
{
    static void UpdateVegetationSpawn(
        const RetainedEntities& sys, const RetainedEntity& obj,
        SceneEngine::VegetationSpawnManager& mgr)
    {
        using namespace SceneEngine;

        static const auto BaseGridSpacing = ParameterBox::MakeParameterNameHash("BaseGridSpacing");
        static const auto JitterAmount = ParameterBox::MakeParameterNameHash("JitterAmount");
        static const auto MaxDrawDistance = ParameterBox::MakeParameterNameHash("MaxDrawDistance");
        static const auto FrequencyWeight = ParameterBox::MakeParameterNameHash("FrequencyWeight");
        static const auto NoSpawnWeight = ParameterBox::MakeParameterNameHash("NoSpawnWeight");
        static const auto SuppressionThreshold = ParameterBox::MakeParameterNameHash("SuppressionThreshold");
        static const auto SuppressionNoise = ParameterBox::MakeParameterNameHash("SuppressionNoise");
        static const auto SuppressionGain = ParameterBox::MakeParameterNameHash("SuppressionGain");
        static const auto SuppressionLacunarity = ParameterBox::MakeParameterNameHash("SuppressionLacunarity");
        static const auto Model = ParameterBox::MakeParameterNameHash("Model");
        static const auto Material = ParameterBox::MakeParameterNameHash("Material");

        VegetationSpawnConfig cfg;
        cfg._baseGridSpacing = obj._properties.GetParameter(BaseGridSpacing, cfg._baseGridSpacing);
        cfg._jitterAmount = obj._properties.GetParameter(JitterAmount, cfg._jitterAmount);

        for (auto cid:obj._children) {
            const auto* child = sys.GetEntity(obj._doc, cid);
            if (!child) continue;

            VegetationSpawnConfig::Material material;
            material._noSpawnWeight = child->_properties.GetParameter(NoSpawnWeight, material._noSpawnWeight);
            material._suppressionThreshold = child->_properties.GetParameter(SuppressionThreshold, material._suppressionThreshold);
            material._suppressionNoise = child->_properties.GetParameter(SuppressionNoise, material._suppressionNoise);
            material._suppressionGain = child->_properties.GetParameter(SuppressionGain, material._suppressionGain);
            material._suppressionLacunarity = child->_properties.GetParameter(SuppressionLacunarity, material._suppressionLacunarity);

            for (auto cid:child->_children) {
                const auto* objType = sys.GetEntity(obj._doc, cid);
                if (!objType) continue;

                VegetationSpawnConfig::ObjectType objTypeBucket;
                objTypeBucket._modelName = objType->_properties.GetString<::Assets::ResChar>(Model);
                objTypeBucket._materialName = objType->_properties.GetString<::Assets::ResChar>(Material);

                if (objTypeBucket._modelName.empty() || objTypeBucket._materialName.empty())
                    continue;

                unsigned objectTypeIndex = ~0u;
                for (unsigned c=0; c<cfg._objectTypes.size(); ++c)
                    if (    cfg._objectTypes[c]._modelName == objTypeBucket._modelName
                        &&  cfg._objectTypes[c]._materialName == objTypeBucket._materialName) {
                        objectTypeIndex = c;
                        break;
                    }

                if (objectTypeIndex == ~0u) {
                    objectTypeIndex = (unsigned)cfg._objectTypes.size();
                    cfg._objectTypes.push_back(objTypeBucket);
                }

                VegetationSpawnConfig::Bucket bucket;
                bucket._maxDrawDistance = objType->_properties.GetParameter(MaxDrawDistance, bucket._maxDrawDistance);
                bucket._frequencyWeight = objType->_properties.GetParameter(FrequencyWeight, bucket._frequencyWeight);
                bucket._objectType = objectTypeIndex;
                
                material._buckets.push_back(bucket);
            }

            if (!material._buckets.empty())
                cfg._materials.push_back(material);
        }

        mgr.Load(cfg);
    }

    void RegisterVegetationSpawnFlexObjects(
        RetainedEntities& flexSys, 
        std::shared_ptr<SceneEngine::VegetationSpawnManager> spawnManager)
    {
        std::weak_ptr<SceneEngine::VegetationSpawnManager> weakPtrToManager = spawnManager;
        flexSys.RegisterCallback(
            flexSys.GetTypeId((const utf8*)"VegetationSpawnConfig"),
            [weakPtrToManager](const RetainedEntities& flexSys, const Identifier& obj)
            {
                auto mgr = weakPtrToManager.lock();
                if (!mgr) return;

                auto* object = flexSys.GetEntity(obj);
                if (object)
                    UpdateVegetationSpawn(flexSys, *object, *mgr);
            }
        );
    }
}

