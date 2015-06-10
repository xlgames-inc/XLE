// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Format.h"
#include <utility>

namespace RenderCore { namespace ColladaConversion
{

    namespace ProcessingFlags 
    { 
        enum Enum { TexCoordFlip = 1<<0, TangentHandinessFlip = 1<<1, BitangentFlip = 1<<2, Renormalize = 1<<3 };
        typedef unsigned BitField;
    }

    size_t CreateTriangleWindingFromPolygon(size_t polygonVertexCount, unsigned buffer[], size_t bufferCount);

    enum class ComponentType { Float32, Float16, UNorm8 };
    std::pair<ComponentType, unsigned> BreakdownFormat(Metal::NativeFormat::Enum fmt);

    class IVertexSourceData
    {
    public:
        virtual const void* GetData() const = 0;
        virtual size_t GetDataSize() const = 0;
        virtual RenderCore::Metal::NativeFormat::Enum GetFormat() const = 0;
        virtual size_t GetStride() const = 0;
        virtual size_t GetCount() const = 0;
        virtual ProcessingFlags::BitField GetProcessingFlags() const = 0;

        virtual ~IVertexSourceData();
    };

    std::shared_ptr<IVertexSourceData> CreateRawDataSource(
        const void* dataBegin, const void* dataEnd, 
        Metal::NativeFormat::Enum srcFormat);

    class NativeVBLayout
    {
    public:
        std::vector<Metal::InputElementDesc> _elements;
        unsigned _vertexStride;
    };

    class MeshDatabaseAdapter
    {
    public:
        size_t _unifiedVertexCount;

        class Stream
        {
        public:
            std::shared_ptr<IVertexSourceData>  _sourceData;
            std::vector<unsigned>       _vertexMap;
            std::string                 _semanticName;
            unsigned                    _semanticIndex;
            Metal::NativeFormat::Enum   _finalVBFormat;
        };
        std::vector<Stream> _streams;

        unsigned    HasElement(const char name[]) const;
        unsigned    FindElement(const char name[], unsigned semanticIndex = 0) const;
        void        RemoveStream(unsigned elementIndex);

        template<typename OutputType>
            OutputType GetUnifiedElement(size_t vertexIndex, unsigned elementIndex) const;

        auto    BuildNativeVertexBuffer(NativeVBLayout& outputLayout) const -> std::unique_ptr<uint8[]>;
        auto    BuildUnifiedVertexIndexToPositionIndex() const -> std::unique_ptr<uint32[]>;

        void    AddStream(  std::shared_ptr<IVertexSourceData> dataSource,
                            std::vector<unsigned>&& vertexMap,
                            const char semantic[], unsigned semanticIndex,
                            Metal::NativeFormat::Enum finalVBFormat);

        MeshDatabaseAdapter();
        ~MeshDatabaseAdapter();

    protected:
        NativeVBLayout BuildDefaultLayout() const;

        void WriteStream(const Stream& stream, const void* dst, Metal::NativeFormat::Enum dstFormat, size_t dstStride) const;
    };

    void GenerateNormalsAndTangents( 
        MeshDatabaseAdapter& mesh, 
        unsigned normalMapTextureCoordinateSemanticIndex,
        const void* rawIb, size_t indexCount, Metal::NativeFormat::Enum ibFormat);

    static const bool Use16BitFloats = true;

}}