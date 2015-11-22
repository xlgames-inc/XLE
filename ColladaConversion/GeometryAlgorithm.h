// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Format.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Assets/ModelScaffoldInternal.h"

namespace RenderCore { namespace Assets { namespace GeoProc { class MeshDatabase; }}}

namespace RenderCore { namespace ColladaConversion
{
    void GenerateNormalsAndTangents( 
        RenderCore::Assets::GeoProc::MeshDatabase& mesh, 
        unsigned normalMapTextureCoordinateSemanticIndex,
        const void* rawIb, size_t indexCount, Metal::NativeFormat::Enum ibFormat);

    void Transform(
        RenderCore::Assets::GeoProc::MeshDatabase& mesh, 
        const Float4x4& transform);

    void RemoveRedundantBitangents(
        RenderCore::Assets::GeoProc::MeshDatabase& mesh);

    void CopyVertexElements(
        void* destinationBuffer,            size_t destinationVertexStride,
        const void* sourceBuffer,           size_t sourceVertexStride,
        const Assets::VertexElement* destinationLayoutBegin,  const Assets::VertexElement* destinationLayoutEnd,
        const Assets::VertexElement* sourceLayoutBegin,       const Assets::VertexElement* sourceLayoutEnd,
        const uint32* reorderingBegin,      const uint32* reorderingEnd );

    unsigned CalculateVertexSize(
        const Assets::VertexElement* layoutBegin,  
        const Assets::VertexElement* layoutEnd);

    unsigned CalculateVertexSize(
        const Metal::InputElementDesc* layoutBegin,  
        const Metal::InputElementDesc* layoutEnd);
}}
