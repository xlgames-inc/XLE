// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/BlockSerializer.h"
#include "../Metal/InputLayout.h"
#include "../Metal/Forward.h"
#include "../../Utility/MemoryUtils.h"

namespace RenderCore { class CameraDesc; class GlobalTransformConstants; }

namespace RenderCore { namespace Assets
{
    RenderCore::Metal::ConstantBufferPacket DefaultMaterialProperties();

    extern Metal::ConstantBufferLayoutElement   GlobalTransform_Elements[];
    extern size_t                               GlobalTransform_ElementsCount;

    extern Metal::ConstantBufferLayoutElement   MaterialProperties_Elements[];
    extern size_t                               MaterialProperties_ElementsCount;

    extern Metal::ConstantBufferLayoutElement   LocalTransform_Elements[];
    extern size_t                               LocalTransform_ElementsCount;

    bool IsDXTNormalMap(const std::string& textureName);

    #pragma pack(push)
    #pragma pack(1)

    class MaterialParameters : noncopyable
    {
    public:
        class ResourceBinding
        {
        public:
            uint64          _bindHash;
            std::string     _resourceName;

            ResourceBinding(uint64 bindHash, const std::string& resourceName) : _bindHash(bindHash), _resourceName(resourceName) {}
            void Serialize(Serialization::NascentBlockSerializer& serializer) const;
        };
        typedef std::vector<ResourceBinding, Serialization::BlockSerializerAllocator<ResourceBinding>> ResourceBindingSet;
        ResourceBindingSet _bindings;

        void Serialize(Serialization::NascentBlockSerializer& serializer) const;

        MaterialParameters();
        MaterialParameters(MaterialParameters&& moveFrom);
        MaterialParameters& operator=(MaterialParameters&& moveFrom);
    };

    #pragma pack(pop)

    static const uint64 ChunkType_ModelScaffold = ConstHash64<'Mode', 'lSca', 'fold'>::Value;
    static const uint64 ChunkType_ModelScaffoldLargeBlocks = ConstHash64<'Mode', 'lSca', 'fold', 'Larg'>::Value;
    static const uint64 ChunkType_AnimationSet = ConstHash64<'Anim', 'Set'>::Value;
    static const uint64 ChunkType_Skeleton = ConstHash64<'Skel', 'eton'>::Value;
}}

