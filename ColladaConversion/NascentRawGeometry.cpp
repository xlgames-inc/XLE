// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentRawGeometry.h"
#include "../Assets/BlockSerializer.h"
#include "../Utility/StringUtils.h"
#include "../Utility/MemoryUtils.h"

void Serialize( Serialization::NascentBlockSerializer& serializer, 
                const RenderCore::Metal::InputElementDesc&  object)
{
    ::Serialize(serializer, object._semanticName);
    ::Serialize(serializer, object._semanticIndex);
    ::Serialize(serializer, unsigned(object._nativeFormat));
    ::Serialize(serializer, object._inputSlot);
    ::Serialize(serializer, object._alignedByteOffset);
    ::Serialize(serializer, unsigned(object._inputSlotClass));
    ::Serialize(serializer, object._instanceDataStepRate);
}

namespace RenderCore { namespace ColladaConversion
{
    GeometryInputAssembly::GeometryInputAssembly(   std::vector<Metal::InputElementDesc>&& vertexInputLayout,
                                                    unsigned vertexStride)
    :       _vertexStride(vertexStride)
    ,       _vertexInputLayout(vertexInputLayout)
    {
    }

    GeometryInputAssembly::GeometryInputAssembly() 
    :   _vertexStride(0)
    {
    }

    void    GeometryInputAssembly::Serialize(Serialization::NascentBlockSerializer& outputSerializer, unsigned slotFilter) const
    {
        Serialization::NascentBlockSerializer subBlock;
        unsigned elementCount = 0;
        for (auto i=_vertexInputLayout.begin(); i!=_vertexInputLayout.end(); ++i) {
            if (slotFilter == ~unsigned(0x0) || i->_inputSlot == slotFilter) {
                char semantic[16];
                XlZeroMemory(semantic);     // make sure unused space is 0
                XlCopyNString(semantic, AsPointer(i->_semanticName.begin()), i->_semanticName.size());
                semantic[dimof(semantic)-1] = '\0';
                for (unsigned c=0; c<dimof(semantic); ++c) { subBlock.SerializeValue((uint8)semantic[c]); }
                subBlock.SerializeValue(i->_semanticIndex);
                subBlock.SerializeValue(unsigned(i->_nativeFormat));
                subBlock.SerializeValue(i->_alignedByteOffset);
                ++elementCount;
            }
        }
        outputSerializer.SerializeSubBlock(subBlock);
        outputSerializer.SerializeValue(elementCount);
        outputSerializer.SerializeValue(_vertexStride);
    }





    NascentRawGeometry::NascentRawGeometry(DynamicArray<uint8>&&    vb,
                                DynamicArray<uint8>&&               ib,
                                GeometryInputAssembly&&             mainDrawInputAssembly,
                                Metal::NativeFormat::Enum           indexFormat,
                                std::vector<NascentDrawCallDesc>&&  mainDrawCalls,
                                DynamicArray<uint32>&&              unifiedVertexIndexToPositionIndex,
                                std::vector<uint64>&&               matBindingSymbols)
    :       _vertices(std::forward<DynamicArray<uint8>>(vb))
    ,       _indices(std::forward<DynamicArray<uint8>>(ib))
    ,       _mainDrawCalls(std::forward<std::vector<NascentDrawCallDesc>>(mainDrawCalls))
    ,       _mainDrawInputAssembly(std::forward<GeometryInputAssembly>(mainDrawInputAssembly))
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

    void    NascentRawGeometry::Serialize(Serialization::NascentBlockSerializer& outputSerializer, std::vector<uint8>& largeResourcesBlock) const
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

        _mainDrawInputAssembly.Serialize(outputSerializer);
        outputSerializer.SerializeValue(unsigned(vbOffset));
        outputSerializer.SerializeValue(unsigned(vbSize));
        outputSerializer.SerializeValue(unsigned(_indexFormat));
        outputSerializer.SerializeValue(unsigned(ibOffset));
        outputSerializer.SerializeValue(unsigned(ibSize));
        
        outputSerializer.SerializeSubBlock(AsPointer(_mainDrawCalls.begin()), AsPointer(_mainDrawCalls.end()));
        outputSerializer.SerializeValue(_mainDrawCalls.size());
    }




    void    NascentDrawCallDesc::Serialize(Serialization::NascentBlockSerializer& outputSerializer) const
    {
        outputSerializer.SerializeValue(_firstIndex);
        outputSerializer.SerializeValue(_indexCount);
        outputSerializer.SerializeValue(_firstVertex);
        outputSerializer.SerializeValue(_boundMaterialIndex);
        outputSerializer.SerializeValue(unsigned(_topology));
    }

}}


