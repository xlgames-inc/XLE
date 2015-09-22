// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentRawGeometry.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../Assets/BlockSerializer.h"
#include "../Utility/StringUtils.h"
#include "../Utility/MemoryUtils.h"

void Serialize(
    Serialization::NascentBlockSerializer& outputSerializer,
    const RenderCore::Assets::DrawCallDesc& drawCall)
{
    outputSerializer.SerializeValue(drawCall._firstIndex);
    outputSerializer.SerializeValue(drawCall._indexCount);
    outputSerializer.SerializeValue(drawCall._firstVertex);
    outputSerializer.SerializeValue(drawCall._subMaterialIndex);
    outputSerializer.SerializeValue(drawCall._topology);
}

void Serialize(
    Serialization::NascentBlockSerializer& outputSerializer,
    const RenderCore::Assets::VertexData& vertexData)
{
    Serialize(outputSerializer, vertexData._ia);
    Serialize(outputSerializer, vertexData._offset);
    Serialize(outputSerializer, vertexData._size);
}

void Serialize(
    Serialization::NascentBlockSerializer& outputSerializer,
    const RenderCore::Assets::IndexData& indexData)
{
    Serialize(outputSerializer, indexData._format);
    Serialize(outputSerializer, indexData._offset);
    Serialize(outputSerializer, indexData._size);
}

void Serialize(
    Serialization::NascentBlockSerializer& outputSerializer,
    const RenderCore::Assets::GeoInputAssembly& ia)
{
    outputSerializer.SerializeRaw(ia._elements);
    Serialize(outputSerializer, ia._vertexStride);
}

namespace RenderCore { namespace ColladaConversion
{
    GeoInputAssembly CreateGeoInputAssembly(   
        const std::vector<Metal::InputElementDesc>& vertexInputLayout,
        unsigned vertexStride)
    { 
        GeoInputAssembly result;
        result._vertexStride = vertexStride;
        result._elements.reserve(vertexInputLayout.size());
        for (auto i=vertexInputLayout.begin(); i!=vertexInputLayout.end(); ++i) {
            RenderCore::Assets::VertexElement ele;
            XlZeroMemory(ele);     // make sure unused space is 0
            XlCopyNString(ele._semanticName, AsPointer(i->_semanticName.begin()), i->_semanticName.size());
            ele._semanticName[dimof(ele._semanticName)-1] = '\0';
            ele._semanticIndex = i->_semanticIndex;
            ele._nativeFormat = i->_nativeFormat;
            ele._alignedByteOffset = i->_alignedByteOffset;
            result._elements.push_back(ele);
        }
        return std::move(result);
    }

    NascentRawGeometry::NascentRawGeometry(
        DynamicArray<uint8>&&       vb,
        DynamicArray<uint8>&&       ib,
        GeoInputAssembly&&          mainDrawInputAssembly,
        NativeFormatPlaceholder     indexFormat,
        std::vector<DrawCallDesc>&& mainDrawCalls,
        DynamicArray<uint32>&&      unifiedVertexIndexToPositionIndex,
        std::vector<uint64>&&       matBindingSymbols)
    :       _vertices(std::forward<DynamicArray<uint8>>(vb))
    ,       _indices(std::forward<DynamicArray<uint8>>(ib))
    ,       _mainDrawCalls(std::forward<std::vector<DrawCallDesc>>(mainDrawCalls))
    ,       _mainDrawInputAssembly(std::forward<GeoInputAssembly>(mainDrawInputAssembly))
    ,       _indexFormat(indexFormat)
    ,       _unifiedVertexIndexToPositionIndex(std::forward<DynamicArray<uint32>>(unifiedVertexIndexToPositionIndex))
    ,       _matBindingSymbols(std::forward<std::vector<uint64>>(matBindingSymbols))
    {
    }

    NascentRawGeometry::NascentRawGeometry(NascentRawGeometry&& moveFrom)
    :       _vertices(std::move(moveFrom._vertices))
    ,       _indices(std::move(moveFrom._indices))
    ,       _mainDrawInputAssembly(std::move(moveFrom._mainDrawInputAssembly))
    ,       _indexFormat(moveFrom._indexFormat)
    ,       _mainDrawCalls(std::move(moveFrom._mainDrawCalls))
    ,       _unifiedVertexIndexToPositionIndex(std::move(moveFrom._unifiedVertexIndexToPositionIndex))
    ,       _matBindingSymbols(std::move(moveFrom._matBindingSymbols))
    {
    }

    NascentRawGeometry& NascentRawGeometry::operator=(NascentRawGeometry&& moveFrom)
    {
        _vertices = std::move(moveFrom._vertices);
        _indices = std::move(moveFrom._indices);
        _mainDrawInputAssembly = std::move(moveFrom._mainDrawInputAssembly);
        _indexFormat = moveFrom._indexFormat;
        _mainDrawCalls = std::move(moveFrom._mainDrawCalls);
        _unifiedVertexIndexToPositionIndex = std::move(moveFrom._unifiedVertexIndexToPositionIndex);
        _matBindingSymbols = std::move(moveFrom._matBindingSymbols);
        return *this;
    }

    NascentRawGeometry::NascentRawGeometry()
    : _vertices(nullptr, 0)
    , _indices(nullptr, 0)
    , _unifiedVertexIndexToPositionIndex(nullptr, 0)
    {
        _indexFormat = Metal::NativeFormat::Unknown;
    }

    void NascentRawGeometry::Serialize(
        Serialization::NascentBlockSerializer& outputSerializer, 
        std::vector<uint8>& largeResourcesBlock) const
    {
            //  We're going to write the index and vertex buffer data to the "large resources block"
            //  class members and scaffold structure get written to the serialiser, but the very large stuff
            //  should end up in a separate pool

        auto vbOffset = largeResourcesBlock.size();
        auto vbSize = _vertices.size();
        largeResourcesBlock.insert(largeResourcesBlock.end(), _vertices.begin(), _vertices.end());

        auto ibOffset = largeResourcesBlock.size();
        auto ibSize = _indices.size();
        largeResourcesBlock.insert(largeResourcesBlock.end(), _indices.begin(), _indices.end());

        ::Serialize(
            outputSerializer, 
            RenderCore::Assets::VertexData 
                { _mainDrawInputAssembly, unsigned(vbOffset), unsigned(vbSize) });

        ::Serialize(
            outputSerializer, 
            RenderCore::Assets::IndexData 
                { unsigned(_indexFormat), unsigned(ibOffset), unsigned(ibSize) });
        
        outputSerializer.SerializeSubBlock(AsPointer(_mainDrawCalls.begin()), AsPointer(_mainDrawCalls.end()));
        outputSerializer.SerializeValue(_mainDrawCalls.size());
    }




    

}}


