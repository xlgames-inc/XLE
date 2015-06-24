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
        static const auto Model = ParameterBox::MakeParameterNameHash("Model");
        static const auto Material = ParameterBox::MakeParameterNameHash("Material");

        VegetationSpawnConfig cfg;
        cfg._baseGridSpacing = obj._properties.GetParameter(BaseGridSpacing, cfg._baseGridSpacing);

        for (auto cid:obj._children) {
            const auto* child = sys.GetEntity(obj._doc, cid);
            if (!child) continue;

            VegetationSpawnConfig::Bucket bucket;
            bucket._jitterAmount = child->_properties.GetParameter(JitterAmount, bucket._jitterAmount);
            bucket._maxDrawDistance = child->_properties.GetParameter(MaxDrawDistance, bucket._maxDrawDistance);
            bucket._modelName = child->_properties.GetString<::Assets::ResChar>(Model);
            bucket._materialName = child->_properties.GetString<::Assets::ResChar>(Material);
            if (!bucket._modelName.empty() && !bucket._materialName.empty())
                cfg._buckets.push_back(bucket);
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

