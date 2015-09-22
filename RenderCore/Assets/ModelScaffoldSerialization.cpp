// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelScaffoldInternal.h"
#include "../../Assets/BlockSerializer.h"

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
