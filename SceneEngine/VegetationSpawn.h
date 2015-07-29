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
        class ObjectType
        {
        public:
            ::Assets::rstring _modelName, _materialName;

            ObjectType();
            ObjectType(const ParameterBox&);
        };

        class Bucket
        {
        public:
            unsigned _objectType;
            float _maxDrawDistance;
            float _frequencyWeight;
            Bucket();
            Bucket(const ParameterBox&);
        };

        class Material
        {
        public:
            std::vector<Bucket> _buckets;
            float _noSpawnWeight;
            float _suppressionThreshold;
            float _suppressionNoise;
            float _suppressionGain;
            float _suppressionLacunarity;
            Material();
            Material(const ParameterBox&);
        };

        float _baseGridSpacing;
        float _jitterAmount;
        std::vector<Material> _materials;
        std::vector<ObjectType> _objectTypes;

        ::Assets::DirectorySearchRules _searchRules;

        VegetationSpawnConfig(
            InputStreamFormatter<utf8>& formatter,
            const ::Assets::DirectorySearchRules& searchRules);
        VegetationSpawnConfig();
        ~VegetationSpawnConfig();

        void        Write(OutputStreamFormatter& formatter) const;
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

