// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Format.h"
#include "../RenderCore/Metal/InputLayout.h"

namespace RenderCore { namespace ColladaConversion
{
    class MeshDatabaseAdapter;

    void GenerateNormalsAndTangents( 
        MeshDatabaseAdapter& mesh, 
        unsigned normalMapTextureCoordinateSemanticIndex,
        const void* rawIb, size_t indexCount, Metal::NativeFormat::Enum ibFormat);

    void CopyVertexElements(
        void* destinationBuffer,            size_t destinationVertexStride,
        const void* sourceBuffer,           size_t sourceVertexStride,
        const Metal::InputElementDesc* destinationLayoutBegin,  const Metal::InputElementDesc* destinationLayoutEnd,
        const Metal::InputElementDesc* sourceLayoutBegin,       const Metal::InputElementDesc* sourceLayoutEnd,
        const uint16* reorderingBegin,      const uint16* reorderingEnd );

    unsigned CalculateVertexSize(
        const Metal::InputElementDesc* layoutBegin,  
        const Metal::InputElementDesc* layoutEnd);
}}
