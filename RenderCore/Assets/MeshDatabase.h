// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Metal/Format.h"
#include "../Metal/InputLayout.h"
#include <utility>
#include <memory>
#include <vector>
#include <string>

namespace RenderCore { namespace Assets { namespace GeoProc
{
    namespace ProcessingFlags 
    {
        enum Enum { TexCoordFlip = 1<<0, TangentHandinessFlip = 1<<1, BitangentFlip = 1<<2, Renormalize = 1<<3 };
        typedef unsigned BitField;
    }

    namespace FormatHint
    {
        enum Enum { IsColor = 1<<0 };
        typedef unsigned BitField;
    }

    class IVertexSourceData;
    class NativeVBLayout;

    /// <summary>A representation of a mesh used during geometry processing</summary>
    /// MeshDatabase provides some utilities and structure that can be used in
    /// geometry processing operations (such as redundant vertex removal and mesh
    /// optimisation)
    /// A lot of flexibility is allowed for how the source mesh data is stored -- whi
    class MeshDatabase
    {
    public:
        unsigned    HasElement(const char name[]) const;
        unsigned    FindElement(const char name[], unsigned semanticIndex = 0) const;
        void        RemoveStream(unsigned elementIndex);

        template<typename OutputType>
            OutputType GetUnifiedElement(size_t vertexIndex, unsigned elementIndex) const;
        size_t GetUnifiedVertexCount() const { return _unifiedVertexCount; }

        auto    BuildNativeVertexBuffer(const NativeVBLayout& outputLayout) const   -> std::unique_ptr<uint8[]>;
        auto    BuildUnifiedVertexIndexToPositionIndex() const                      -> std::unique_ptr<uint32[]>;

        void    AddStream(
            std::shared_ptr<IVertexSourceData> dataSource,
            std::vector<unsigned>&& vertexMap,
            const char semantic[], unsigned semanticIndex);

        class Stream;
        const Stream&       GetStream(unsigned index) const { return _streams[index]; }
        unsigned            GetStreamCount() const          { return (unsigned)_streams.size(); }

        MeshDatabase();
        ~MeshDatabase();

        class Stream
        {
        public:
            const IVertexSourceData& GetSourceData() const      { return *_sourceData; }
            const std::vector<unsigned>& GetVertexMap() const   { return _vertexMap; }
            const std::string& GetSemanticName() const          { return _semanticName; }
            const unsigned GetSemanticIndex() const             { return _semanticIndex; }

            Stream();
            Stream(
                std::shared_ptr<IVertexSourceData> sourceData, std::vector<unsigned> vertexMap, 
                const std::string& semanticName, unsigned semanticIndex);
            Stream(Stream&& moveFrom) never_throws;
            Stream& operator=(Stream&& moveFrom) never_throws;
            ~Stream();
        private:
            std::shared_ptr<IVertexSourceData>  _sourceData;
            std::vector<unsigned>   _vertexMap;
            std::string             _semanticName;
            unsigned                _semanticIndex;
        };

    private:
        size_t  _unifiedVertexCount;

        std::vector<Stream> _streams;

        void WriteStream(
            const Stream& stream, const void* dst, 
            Metal::NativeFormat::Enum dstFormat, size_t dstStride) const;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    class IVertexSourceData
    {
    public:
        virtual const void* GetData() const = 0;
        virtual size_t GetDataSize() const = 0;
        virtual RenderCore::Metal::NativeFormat::Enum GetFormat() const = 0;

        virtual size_t GetStride() const = 0;
        virtual size_t GetCount() const = 0;
        virtual ProcessingFlags::BitField GetProcessingFlags() const = 0;
        virtual FormatHint::BitField GetFormatHint() const = 0;

        virtual ~IVertexSourceData();
    };

    class NativeVBLayout
    {
    public:
        std::vector<Metal::InputElementDesc> _elements;
        unsigned _vertexStride;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    std::shared_ptr<IVertexSourceData>
        CreateRawDataSource(
            const void* dataBegin, const void* dataEnd, 
            size_t count, size_t stride,
            Metal::NativeFormat::Enum srcFormat);

    std::shared_ptr<IVertexSourceData>
        CreateRawDataSource(
            const void* dataBegin, const void* dataEnd, 
            Metal::NativeFormat::Enum srcFormat);

    class NativeVBSettings
    {
    public:
        bool    _use16BitFloats;
    };

    NativeVBLayout BuildDefaultLayout(MeshDatabase& mesh, const NativeVBSettings& settings);

    /// <summary>Creates a triangle winding order for a convex polygon with the given number of points<summary>
    /// If we have a convex polygon, this can be used to convert it into a list of triangles.
    /// However, note that if the polygon is concave, then some bad triangles will be created.
    size_t CreateTriangleWindingFromPolygon(
        unsigned buffer[], size_t bufferCount, size_t polygonVertexCount);

}}}

