// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Format.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"  // for topology
#include <vector>

namespace COLLADAFW { class Geometry; }
namespace Serialization { class NascentBlockSerializer; }

namespace RenderCore { namespace ColladaConversion
{
    class GeometryInputAssembly
    {
    public:
        std::vector<Metal::InputElementDesc>    _vertexInputLayout;
        unsigned                                _vertexStride;

        void    Serialize(Serialization::NascentBlockSerializer& outputSerializer, unsigned slotFilter = ~unsigned(0x0)) const;

        GeometryInputAssembly();
        GeometryInputAssembly(std::vector<Metal::InputElementDesc>&& vertexInputLayout, unsigned vertexStride);
    };

        ////////////////////////////////////////////////////////

    class NascentDrawCallDesc
    {
    public:
        unsigned    _firstIndex, _indexCount;
        unsigned    _firstVertex;
        unsigned    _subMaterialIndex;
        Metal::Topology::Enum   _topology;

        void    Serialize(Serialization::NascentBlockSerializer& outputSerializer) const;

        NascentDrawCallDesc(unsigned firstIndex, unsigned indexCount, unsigned firstVertex, unsigned subMaterialIndex, Metal::Topology::Enum topology) 
        : _firstIndex(firstIndex), _indexCount(indexCount), _firstVertex(firstVertex), _subMaterialIndex(subMaterialIndex), _topology(topology) {}
    };

        ////////////////////////////////////////////////////////

    class NascentRawGeometry
    {
    public:
        NascentRawGeometry(     DynamicArray<uint8>&& vb, DynamicArray<uint8>&& ib,
                                GeometryInputAssembly&&             mainDrawInputAssembly,
                                Metal::NativeFormat::Enum           indexFormat,
                                std::vector<NascentDrawCallDesc>&&  mainDrawCalls,
                                DynamicArray<uint32>&&              unifiedVertexIndexToPositionIndex,
                                std::vector<uint64>&&               materials);
        NascentRawGeometry(NascentRawGeometry&& moveFrom);
        NascentRawGeometry& operator=(NascentRawGeometry&& moveFrom);
        NascentRawGeometry();

        void    Serialize(Serialization::NascentBlockSerializer& outputSerializer, std::vector<uint8>& largeResourcesBlock) const;

        DynamicArray<uint8>     _vertices;
        DynamicArray<uint8>     _indices;

        GeometryInputAssembly               _mainDrawInputAssembly;
        Metal::NativeFormat::Enum           _indexFormat;
        std::vector<NascentDrawCallDesc>    _mainDrawCalls;
        std::vector<uint64>                 _materials;

            //  Only required during processing
        DynamicArray<uint32>    _unifiedVertexIndexToPositionIndex;

    private:
        NascentRawGeometry(NascentRawGeometry&);
    };

        ////////////////////////////////////////////////////////

    NascentRawGeometry Convert(const COLLADAFW::Geometry* geometry);

}}