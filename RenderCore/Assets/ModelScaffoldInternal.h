// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"
#include "../../Utility/Streams/Serialization.h"
#include "../../Core/Types.h"

namespace RenderCore { namespace Assets 
{
    typedef uint64 MaterialGuid;
    typedef unsigned TopologyPlaceholder;
    typedef unsigned NativeFormatPlaceholder;

    #pragma pack(push)
    #pragma pack(1)

////////////////////////////////////////////////////////////////////////////////////////////
    //      g e o m e t r y         //

    class ModelCommandStream
    {
    public:
            //  "Geo calls" & "draw calls". Geo calls have 
            //  a vertex buffer and index buffer, and contain
            //  draw calls within them.
        class GeoCall
        {
        public:
            unsigned        _geoId;
            unsigned        _transformMarker;
            MaterialGuid*   _materialGuids;
            size_t          _materialCount;
            unsigned        _levelOfDetail;
        };

        class InputInterface
        {
        public:
            uint64*     _jointNames;
            size_t      _jointCount;
        };

            /////   -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-   /////
        const GeoCall&      GetGeoCall(size_t index) const;
        size_t              GetGeoCallCount() const;

        const GeoCall&      GetSkinCall(size_t index) const;
        size_t              GetSkinCallCount() const;

        const InputInterface&   GetInputInterface() const { return _inputInterface; }

        ~ModelCommandStream();
    private:
        GeoCall*        _geometryInstances;
        size_t          _geometryInstanceCount;
        GeoCall*        _skinControllerInstances;
        size_t          _skinControllerInstanceCount;
        InputInterface  _inputInterface;

        ModelCommandStream(const ModelCommandStream&) = delete;
        ModelCommandStream& operator=(const ModelCommandStream&) = delete;
    };

    inline auto         ModelCommandStream::GetGeoCall(size_t index) const -> const GeoCall&    { return _geometryInstances[index]; }
    inline size_t       ModelCommandStream::GetGeoCallCount() const                             { return _geometryInstanceCount; }
    inline auto         ModelCommandStream::GetSkinCall(size_t index) const -> const GeoCall&   { return _skinControllerInstances[index]; }
    inline size_t       ModelCommandStream::GetSkinCallCount() const                            { return _skinControllerInstanceCount; }

    class DrawCallDesc
    {
    public:
        unsigned    _firstIndex, _indexCount;
        unsigned    _firstVertex;
        unsigned    _subMaterialIndex;
        TopologyPlaceholder    _topology;

        DrawCallDesc(
            unsigned firstIndex, unsigned indexCount, unsigned firstVertex, unsigned subMaterialIndex, 
            TopologyPlaceholder topology) 
        : _firstIndex(firstIndex), _indexCount(indexCount), _firstVertex(firstVertex)
        , _subMaterialIndex(subMaterialIndex), _topology(topology) {}
    };

    class VertexElement
    {
    public:
        char            _semantic[16];  // limited max size for semantic name (only alternative is to use a hash value)
        unsigned        _semanticIndex;
        NativeFormatPlaceholder    _format;
        unsigned        _startOffset;

        VertexElement() {}
        VertexElement(const VertexElement&) = delete;
        VertexElement& operator=(const VertexElement&) = delete;
    };

    class GeoInputAssembly
    {
    public:
        VertexElement*  _elements;
        unsigned        _elementCount;
        unsigned        _vertexStride;

        uint64 BuildHash() const;

        ~GeoInputAssembly();
    };

    class VertexData
    {
    public:
        GeoInputAssembly    _ia;
        unsigned            _offset, _size;
    };

    class IndexData
    {
    public:
        NativeFormatPlaceholder        _format;
        unsigned            _offset, _size;
    };

    class RawGeometry
    {
    public:
        VertexData      _vb;
        IndexData       _ib;

            // Draw calls
        DrawCallDesc*   _drawCalls;
        size_t          _drawCallsCount;

        ~RawGeometry();
    private:
        RawGeometry();

        RawGeometry(const RawGeometry&) = delete;
        RawGeometry& operator=(const RawGeometry&) = delete;
    };

    class BoundSkinnedGeometry : public RawGeometry
    {
    public:

            //  The "RawGeometry" base class contains the 
            //  unanimated vertex elements (and draw calls for
            //  rendering the object as a whole)
        VertexData      _animatedVertexElements;
        VertexData      _skeletonBinding;

        Float4x4*       _inverseBindMatrices;
        size_t          _inverseBindMatrixCount;
        Float4x4*       _inverseBindByBindShapeMatrices;
        size_t          _inverseBindByBindShapeMatrixCount;
        uint16*         _jointMatrices;         // (uint16 or uint8 for this array)
        size_t          _jointMatrixCount;
        Float4x4        _bindShapeMatrix;

        DrawCallDesc*   _preskinningDrawCalls;
        size_t          _preskinningDrawCallCount;

        std::pair<Float3, Float3>   _localBoundingBox;

        ~BoundSkinnedGeometry();
    private:
        BoundSkinnedGeometry();
    };

    #pragma pack(pop)

}}

