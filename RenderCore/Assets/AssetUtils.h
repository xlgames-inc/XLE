// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Metal/Forward.h"
#include "../Metal/InputLayout.h"
#include "../../Utility/MemoryUtils.h"      // (for ConstHash64)

namespace RenderCore { namespace Techniques { class CameraDesc; class GlobalTransformConstants; } }

namespace RenderCore { namespace Assets
{
    extern Metal::ConstantBufferLayoutElement   GlobalTransform_Elements[];
    extern size_t                               GlobalTransform_ElementsCount;

    extern Metal::ConstantBufferLayoutElement   MaterialProperties_Elements[];
    extern size_t                               MaterialProperties_ElementsCount;

    extern Metal::ConstantBufferLayoutElement   LocalTransform_Elements[];
    extern size_t                               LocalTransform_ElementsCount;

    bool IsDXTNormalMap(const std::string& textureName);

    static const uint64 ChunkType_ModelScaffold = ConstHash64<'Mode', 'lSca', 'fold'>::Value;
    static const uint64 ChunkType_ModelScaffoldLargeBlocks = ConstHash64<'Mode', 'lSca', 'fold', 'Larg'>::Value;
    static const uint64 ChunkType_AnimationSet = ConstHash64<'Anim', 'Set'>::Value;
    static const uint64 ChunkType_Skeleton = ConstHash64<'Skel', 'eton'>::Value;
    static const uint64 ChunkType_RawMat = ConstHash64<'RawM', 'at'>::Value;
}}

