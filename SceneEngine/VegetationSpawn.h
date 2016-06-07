// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "LightingParser.h"
#include "../RenderCore/IThreadContext_Forward.h"
#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/Assets/DelayedDrawCall.h"       // for DelayStep
#include "../Assets/AssetsCore.h"
#include "../Assets/AssetUtils.h"
#include "../Utility/UTFUtils.h"

namespace RenderCore { namespace Assets { class ModelCache; }}
namespace Utility
{
    template<typename Type> class InputStreamFormatter;
    class OutputStreamFormatter;
}

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
        };

        class Bucket
        {
        public:
            unsigned _objectType;
            float _maxDrawDistance;
            float _frequencyWeight;
            Bucket();
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
            unsigned _materialId;
            Material();
        };

        float _baseGridSpacing;
        float _jitterAmount;
        bool _alignToTerrainUp;
        std::vector<Material> _materials;
        std::vector<ObjectType> _objectTypes;

        ::Assets::DirectorySearchRules _searchRules;

        VegetationSpawnConfig(
            Utility::InputStreamFormatter<utf8>& formatter,
            const ::Assets::DirectorySearchRules& searchRules);
        VegetationSpawnConfig();
        ~VegetationSpawnConfig();

        void        Write(OutputStreamFormatter& formatter) const;
    };

    class LightingParserContext;
    class VegetationSpawnResources;
    class PreparedScene;

    void VegetationSpawn_Prepare(
        RenderCore::IThreadContext& context,
        LightingParserContext& parserContext,
        PreparedScene& preparedScene,
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
            RenderCore::Metal::DeviceContext& context, 
            LightingParserContext& lightingParserContext,
            unsigned techniqueIndex,
            RenderCore::Assets::DelayStep delayStep);
        bool HasContent(RenderCore::Assets::DelayStep delayStep) const;

        std::shared_ptr<ILightingParserPlugin> GetParserPlugin();

        void Load(const VegetationSpawnConfig& cfg);
        void Reset();

        const VegetationSpawnConfig& GetConfig() const;

        VegetationSpawnManager(
            std::shared_ptr<RenderCore::Assets::ModelCache> modelCache);
        ~VegetationSpawnManager();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
        friend class VegetationSpawnPlugin;
    };
}

