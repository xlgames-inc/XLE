// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "LightingParser.h"
#include "../RenderCore/Metal/Forward.h"
#include "../Assets/AssetsCore.h"

namespace RenderCore { namespace Assets { class ModelCache; }}

namespace SceneEngine
{
    class VegetationSpawnConfig
    {
    public:
        class Bucket
        {
        public:
            ::Assets::rstring _modelName, _materialName;
            float _jitterAmount;
            float _maxDrawDistance;
            Bucket();
        };
        std::vector<Bucket> _buckets;
        float _baseGridSpacing;

        VegetationSpawnConfig(const ::Assets::ResChar src[]);
        VegetationSpawnConfig();
        ~VegetationSpawnConfig();
    };

    class LightingParserContext;
    class VegetationSpawnResources;

    void VegetationSpawn_Prepare(
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& lightingParserContext,
        const VegetationSpawnConfig& cfg,
        VegetationSpawnResources& resources);

    bool VegetationSpawn_DrawInstances(
        RenderCore::Metal::DeviceContext* context,
        VegetationSpawnResources& resources,
        unsigned instanceId, unsigned indexCount, 
        unsigned startIndexLocation, unsigned baseVertexLocation);

    class VegetationSpawnManager
    {
    public:
        void Render(
            RenderCore::Metal::DeviceContext* context, 
            LightingParserContext& lightingParserContext,
            unsigned techniqueIndex);

        std::shared_ptr<ILightingParserPlugin> GetParserPlugin();

        void Load(const VegetationSpawnConfig& cfg);
        void Reset();

        VegetationSpawnManager(
            std::shared_ptr<RenderCore::Assets::ModelCache> modelCache);
        ~VegetationSpawnManager();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
        friend class VegetationSpawnPlugin;
    };
}

