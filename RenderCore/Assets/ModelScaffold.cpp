// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelRunTime.h"
#include "ModelScaffoldInternal.h"
#include "ModelImmutableData.h"
#include "AssetUtils.h"
#include "../../Assets/ChunkFileAsset.h"
#include "../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Assets
{
    static const unsigned ModelScaffoldVersion = 1;
    static const unsigned ModelScaffoldLargeBlocksVersion = 0;

    template <typename Type>
        void DestroyArray(const Type* begin, const Type* end)
        {
            for (auto i=begin; i!=end; ++i) { i->~Type(); }
        }

////////////////////////////////////////////////////////////////////////////////

        /// This DestroyArray stuff is too awkward! We could use a serializable vector instead
    ModelCommandStream::~ModelCommandStream()
    {
        DestroyArray(_geometryInstances,        &_geometryInstances[_geometryInstanceCount]);
        DestroyArray(_skinControllerInstances,  &_skinControllerInstances[_skinControllerInstanceCount]);
    }

    BoundSkinnedGeometry::~BoundSkinnedGeometry() {}

    ModelImmutableData::~ModelImmutableData()
    {
        DestroyArray(_geos, &_geos[_geoCount]);
        DestroyArray(_boundSkinnedControllers, &_boundSkinnedControllers[_boundSkinnedControllerCount]);
    }

        ////////////////////////////////////////////////////////////

    uint64 GeoInputAssembly::BuildHash() const
    {
            //  Build a hash for this object.
            //  Note that we should be careful that we don't get an
            //  noise from characters in the left-over space in the
            //  semantic names. Do to this right, we should make sure
            //  that left over space has no effect.
        auto elementsHash = Hash64(AsPointer(_elements.cbegin()), AsPointer(_elements.cend()));
        elementsHash ^= uint64(_vertexStride);
        return elementsHash;
    }

    GeoInputAssembly::GeoInputAssembly() { _vertexStride = 0; }
    GeoInputAssembly::GeoInputAssembly(GeoInputAssembly&& moveFrom)
    :   _elements(std::move(moveFrom._elements))
    ,   _vertexStride(moveFrom._vertexStride)
    {}
    GeoInputAssembly& GeoInputAssembly::operator=(GeoInputAssembly&& moveFrom)
    {
        _elements = std::move(moveFrom._elements);
        _vertexStride = moveFrom._vertexStride;
        return *this;
    }
    GeoInputAssembly::~GeoInputAssembly() {}

    VertexElement::VertexElement()
    {
        _nativeFormat = Format(0); _alignedByteOffset = 0; _semanticIndex = 0;
        XlZeroMemory(_semanticName);
    }

    VertexElement::VertexElement(const VertexElement& ele)
    {
        _nativeFormat = ele._nativeFormat; _alignedByteOffset = ele._alignedByteOffset; _semanticIndex = ele._semanticIndex;
        XlCopyMemory(_semanticName, ele._semanticName, sizeof(_semanticName));
    }

    VertexElement& VertexElement::operator=(const VertexElement& ele)
    {
        _nativeFormat = ele._nativeFormat; _alignedByteOffset = ele._alignedByteOffset; _semanticIndex = ele._semanticIndex;
        XlCopyMemory(_semanticName, ele._semanticName, sizeof(_semanticName));
        return *this;
    }

    RawGeometry::RawGeometry() {}
    RawGeometry::RawGeometry(RawGeometry&& geo) never_throws
    : _vb(std::move(geo._vb))
    , _ib(std::move(geo._ib))
    , _drawCalls(std::move(geo._drawCalls))
    {}

    RawGeometry& RawGeometry::operator=(RawGeometry&& geo) never_throws
    {
        _vb = std::move(geo._vb);
        _ib = std::move(geo._ib);
        _drawCalls = std::move(geo._drawCalls);
        return *this;
    }

    RawGeometry::~RawGeometry() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    unsigned    ModelScaffold::LargeBlocksOffset() const            
    { 
        Resolve(); 
        return _largeBlocksOffset; 
    }

    const ModelImmutableData&   ModelScaffold::ImmutableData() const                
    {
        Resolve(); 
        return *(const ModelImmutableData*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
    }

    const ModelImmutableData*   ModelScaffold::TryImmutableData() const
    {
        if (!_rawMemoryBlock) return nullptr;
        return (const ModelImmutableData*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
    }

    const ModelCommandStream&       ModelScaffold::CommandStream() const                { return ImmutableData()._visualScene; }
    const TransformationMachine&    ModelScaffold::EmbeddedSkeleton() const             { return ImmutableData()._embeddedSkeleton; }
    std::pair<Float3, Float3>       ModelScaffold::GetStaticBoundingBox(unsigned) const { return ImmutableData()._boundingBox; }
    unsigned                        ModelScaffold::GetMaxLOD() const                    { return ImmutableData()._maxLOD; }

    static const ::Assets::AssetChunkRequest ModelScaffoldChunkRequests[]
    {
        ::Assets::AssetChunkRequest { "Scaffold", ChunkType_ModelScaffold, ModelScaffoldVersion, ::Assets::AssetChunkRequest::DataType::BlockSerializer },
        ::Assets::AssetChunkRequest { "LargeBlocks", ChunkType_ModelScaffoldLargeBlocks, ModelScaffoldLargeBlocksVersion, ::Assets::AssetChunkRequest::DataType::DontLoad }
    };
    
    ModelScaffold::ModelScaffold(const ::Assets::ResChar filename[])
    : ChunkFileAsset("ModelScaffold")
    {
        Prepare(filename, ResolveOp{MakeIteratorRange(ModelScaffoldChunkRequests), &Resolver});
    }

    ModelScaffold::ModelScaffold(std::shared_ptr<::Assets::ICompileMarker>&& marker)
    : ChunkFileAsset("ModelScaffold")
    {
        Prepare(*marker, ResolveOp{MakeIteratorRange(ModelScaffoldChunkRequests), &Resolver}); 
    }

    ModelScaffold::ModelScaffold(ModelScaffold&& moveFrom) never_throws
    : ::Assets::ChunkFileAsset(std::move(moveFrom)) 
    , _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
    , _largeBlocksOffset(moveFrom._largeBlocksOffset)
    {}

    ModelScaffold& ModelScaffold::operator=(ModelScaffold&& moveFrom) never_throws
    {
        ::Assets::ChunkFileAsset::operator=(std::move(moveFrom));
        _rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
        _largeBlocksOffset = moveFrom._largeBlocksOffset;
        return *this;
    }

    ModelScaffold::~ModelScaffold()
    {
        auto* data = TryImmutableData();
        if (data)
            data->~ModelImmutableData();
    }

    void ModelScaffold::Resolver(void* obj, IteratorRange<::Assets::AssetChunkResult*> chunks)
    {
        auto* scaffold = (ModelScaffold*)obj;
        if (scaffold) {
            scaffold->_rawMemoryBlock = std::move(chunks[0]._buffer);
            scaffold->_largeBlocksOffset = chunks[1]._offset;
        }
    }
    
///////////////////////////////////////////////////////////////////////////////////////////////////

    unsigned    ModelSupplementScaffold::LargeBlocksOffset() const            
    { 
        Resolve(); 
        return _largeBlocksOffset; 
    }

    const ModelSupplementImmutableData&   ModelSupplementScaffold::ImmutableData() const                
    {
        Resolve(); 
        return *(const ModelSupplementImmutableData*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
    }

    const ModelSupplementImmutableData*   ModelSupplementScaffold::TryImmutableData() const
    {
        if (!_rawMemoryBlock) return nullptr;
        return (const ModelSupplementImmutableData*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
    }

    static const ::Assets::AssetChunkRequest ModelSupplementScaffoldChunkRequests[]
    {
        ::Assets::AssetChunkRequest { "Scaffold", ChunkType_ModelScaffold, 0, ::Assets::AssetChunkRequest::DataType::BlockSerializer },
        ::Assets::AssetChunkRequest { "LargeBlocks", ChunkType_ModelScaffoldLargeBlocks, 0, ::Assets::AssetChunkRequest::DataType::DontLoad }
    };
    
    ModelSupplementScaffold::ModelSupplementScaffold(const ::Assets::ResChar filename[])
    : ChunkFileAsset("ModelSupplementScaffold")
    {
        Prepare(filename, ResolveOp{MakeIteratorRange(ModelSupplementScaffoldChunkRequests), &Resolver});
    }

    ModelSupplementScaffold::ModelSupplementScaffold(std::shared_ptr<::Assets::ICompileMarker>&& marker)
    : ChunkFileAsset("ModelSupplementScaffold")
    {
        Prepare(*marker, ResolveOp{MakeIteratorRange(ModelSupplementScaffoldChunkRequests), &Resolver});
    }

    ModelSupplementScaffold::ModelSupplementScaffold(ModelSupplementScaffold&& moveFrom)
    : ::Assets::ChunkFileAsset(std::move(moveFrom)) 
    , _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
    , _largeBlocksOffset(moveFrom._largeBlocksOffset)
    {}

    ModelSupplementScaffold& ModelSupplementScaffold::operator=(ModelSupplementScaffold&& moveFrom)
    {
        ::Assets::ChunkFileAsset::operator=(std::move(moveFrom));
        _rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
        _largeBlocksOffset = moveFrom._largeBlocksOffset;
        return *this;
    }

    ModelSupplementScaffold::~ModelSupplementScaffold()
    {
        auto* data = TryImmutableData();
        if (data)
            data->~ModelSupplementImmutableData();
    }

    void ModelSupplementScaffold::Resolver(void* obj, IteratorRange<::Assets::AssetChunkResult*> chunks)
    {
        auto* scaffold = (ModelSupplementScaffold*)obj;
        if (scaffold) {
            scaffold->_rawMemoryBlock = std::move(chunks[0]._buffer);
            scaffold->_largeBlocksOffset = chunks[1]._offset;
        }
    }

}}
