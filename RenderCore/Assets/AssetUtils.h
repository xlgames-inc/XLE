// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../IDevice_Forward.h"
#include "../../Utility/MemoryUtils.h"      // (for ConstHash64)
#include "../../Utility/IteratorUtils.h"
#include <vector>

namespace Utility { class ParameterBox; }
namespace Assets { class DirectorySearchRules; }
namespace RenderCore { class InputElementDesc; }

namespace RenderCore { namespace Assets
{
	IResourcePtr CreateStaticVertexBuffer(IteratorRange<const void*> data);
	IResourcePtr CreateStaticIndexBuffer(IteratorRange<const void*> data);

    /// <summary>Set the RES_HAS technique materials parameter</summary>
    /// There are a few standard RES_HAS shader parameters that are defined by
    /// the resources that are bound to a technique material. This function builds
    /// a parameter box with these parameters set.
    ParameterBox TechParams_SetResHas(
        const ParameterBox& inputMatParameters, const ParameterBox& resBindings,
        const ::Assets::DirectorySearchRules& searchRules);

    static const uint64 ChunkType_ModelScaffold = ConstHash64<'Mode', 'lSca', 'fold'>::Value;
    static const uint64 ChunkType_ModelScaffoldLargeBlocks = ConstHash64<'Mode', 'lSca', 'fold', 'Larg'>::Value;
    static const uint64 ChunkType_AnimationSet = ConstHash64<'Anim', 'Set'>::Value;
    static const uint64 ChunkType_Skeleton = ConstHash64<'Skel', 'eton'>::Value;
    static const uint64 ChunkType_RawMat = ConstHash64<'RawM', 'at'>::Value;
    static const uint64 ChunkType_Metrics = ConstHash64<'Metr', 'ics'>::Value;

    class GeoInputAssembly;
    class DrawCallDesc;
    GeoInputAssembly CreateGeoInputAssembly(   
        const std::vector<InputElementDesc>& vertexInputLayout,
        unsigned vertexStride);
    
    std::ostream& StreamOperator(std::ostream& stream, const GeoInputAssembly& ia);
    std::ostream& StreamOperator(std::ostream& stream, const DrawCallDesc& dc);

    #include "../../Utility/ExposeStreamOp.h"
}}
