// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RetainedEntities.h"
#include "../../SceneEngine/VegetationSpawn.h"
#include "../../Utility/Meta/AccessorSerialize.h"

namespace EntityInterface
{
    static void UpdateVegetationSpawn(
        const RetainedEntities& sys, const RetainedEntity& obj,
        SceneEngine::VegetationSpawnManager& mgr)
    {
        using namespace SceneEngine;

        auto cfg = CreateFromParameters<VegetationSpawnConfig>(obj._properties);

        for (auto cid:obj._children) {
            const auto* child = sys.GetEntity(obj._doc, cid);
            if (!child) continue;

            auto material = CreateFromParameters<VegetationSpawnConfig::Material>(child->_properties);

            for (auto cid2:child->_children) {
                const auto* objType = sys.GetEntity(obj._doc, cid2);
                if (!objType) continue;

                auto objTypeBucket = CreateFromParameters<VegetationSpawnConfig::ObjectType>(objType->_properties);
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

                auto bucket = CreateFromParameters<VegetationSpawnConfig::Bucket>(objType->_properties);
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
            [weakPtrToManager](
                const RetainedEntities& flexSys, const Identifier& obj,
                RetainedEntities::ChangeType changeType)
            {
                auto mgr = weakPtrToManager.lock();
                if (!mgr) return;

                if (changeType == RetainedEntities::ChangeType::Delete) {
                    mgr->Load(SceneEngine::VegetationSpawnConfig());
                    return;
                }

                auto* object = flexSys.GetEntity(obj);
                if (object)
                    UpdateVegetationSpawn(flexSys, *object, *mgr);
            }
        );
    }
}

