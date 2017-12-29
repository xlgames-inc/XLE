// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelRunTime.h"
#include "ModelScaffoldInternal.h"
#include "ModelImmutableData.h"
#include "AssetUtils.h"
#include "../Format.h"
#include "../Types.h"
#include "../../Assets/ChunkFileContainer.h"
#include "../../Assets/DeferredConstruction.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/PtrUtils.h"

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
    GeoInputAssembly::GeoInputAssembly(GeoInputAssembly&& moveFrom) never_throws
    :   _elements(std::move(moveFrom._elements))
    ,   _vertexStride(moveFrom._vertexStride)
    {}
    GeoInputAssembly& GeoInputAssembly::operator=(GeoInputAssembly&& moveFrom) never_throws
    {
        _elements = std::move(moveFrom._elements);
        _vertexStride = moveFrom._vertexStride;
        return *this;
    }
    GeoInputAssembly::~GeoInputAssembly() {}

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
        return _largeBlocksOffset; 
    }

    const ModelImmutableData&   ModelScaffold::ImmutableData() const                
    {
        return *(const ModelImmutableData*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
    }

    const ModelCommandStream&       ModelScaffold::CommandStream() const                { return ImmutableData()._visualScene; }
    const SkeletonMachine&			ModelScaffold::EmbeddedSkeleton() const             { return ImmutableData()._embeddedSkeleton; }
    std::pair<Float3, Float3>       ModelScaffold::GetStaticBoundingBox(unsigned) const { return ImmutableData()._boundingBox; }
    unsigned                        ModelScaffold::GetMaxLOD() const                    { return ImmutableData()._maxLOD; }

	const ::Assets::DepValPtr&					ModelScaffold::GetDependencyValidation() const { return _chunkFile.GetDependencyValidation(); }
	std::shared_ptr<::Assets::IFileInterface>	ModelScaffold::OpenFile() const { return _chunkFile.OpenFile(); }
	const ::Assets::rstring&					ModelScaffold::Filename() const { return _chunkFile.Filename(); }

    static const ::Assets::AssetChunkRequest ModelScaffoldChunkRequests[]
    {
        ::Assets::AssetChunkRequest { "Scaffold", ChunkType_ModelScaffold, ModelScaffoldVersion, ::Assets::AssetChunkRequest::DataType::BlockSerializer },
        ::Assets::AssetChunkRequest { "LargeBlocks", ChunkType_ModelScaffoldLargeBlocks, ModelScaffoldLargeBlocksVersion, ::Assets::AssetChunkRequest::DataType::DontLoad }
    };
    
    ModelScaffold::ModelScaffold(const ::Assets::ChunkFileContainer& chunkFile)
	: _chunkFile(chunkFile)
    {
        auto chunks = chunkFile.ResolveRequests(MakeIteratorRange(ModelScaffoldChunkRequests));
		assert(chunks.size() == 2);
		_rawMemoryBlock = std::move(chunks[0]._buffer);
		_largeBlocksOffset = chunks[1]._offset;
    }

    ModelScaffold::ModelScaffold(ModelScaffold&& moveFrom) never_throws
    : _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
    , _largeBlocksOffset(moveFrom._largeBlocksOffset)
	, _chunkFile(std::move(moveFrom._chunkFile))
    {}

    ModelScaffold& ModelScaffold::operator=(ModelScaffold&& moveFrom) never_throws
    {
		assert(!_rawMemoryBlock);		// (not thread safe to use this operator after we've hit "ready" status
        _rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
        _largeBlocksOffset = moveFrom._largeBlocksOffset;
		_chunkFile = std::move(moveFrom._chunkFile);
        return *this;
    }

    ModelScaffold::~ModelScaffold()
    {
        ImmutableData().~ModelImmutableData();
    }

//////////////////////////////////////////////////////////////////////////////////////////////////

    unsigned    ModelSupplementScaffold::LargeBlocksOffset() const            
    { 
        return _largeBlocksOffset; 
    }

    const ModelSupplementImmutableData&   ModelSupplementScaffold::ImmutableData() const                
    {
        return *(const ModelSupplementImmutableData*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
    }

	const ::Assets::DepValPtr&					ModelSupplementScaffold::GetDependencyValidation() const { return _chunkFile.GetDependencyValidation(); }
	std::shared_ptr<::Assets::IFileInterface>	ModelSupplementScaffold::OpenFile() const { return _chunkFile.OpenFile(); }

    static const ::Assets::AssetChunkRequest ModelSupplementScaffoldChunkRequests[]
    {
        ::Assets::AssetChunkRequest { "Scaffold", ChunkType_ModelScaffold, 0, ::Assets::AssetChunkRequest::DataType::BlockSerializer },
        ::Assets::AssetChunkRequest { "LargeBlocks", ChunkType_ModelScaffoldLargeBlocks, 0, ::Assets::AssetChunkRequest::DataType::DontLoad }
    };
    
    ModelSupplementScaffold::ModelSupplementScaffold(const ::Assets::ChunkFileContainer& chunkFile)
	: _chunkFile(chunkFile)
	{
		auto chunks = chunkFile.ResolveRequests(MakeIteratorRange(ModelSupplementScaffoldChunkRequests));
		assert(chunks.size() == 2);
		_rawMemoryBlock = std::move(chunks[0]._buffer);
		_largeBlocksOffset = chunks[1]._offset;
	}

    ModelSupplementScaffold::ModelSupplementScaffold(ModelSupplementScaffold&& moveFrom) never_throws
    : _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
    , _largeBlocksOffset(moveFrom._largeBlocksOffset)
	, _chunkFile(std::move(moveFrom._chunkFile))
    {}

    ModelSupplementScaffold& ModelSupplementScaffold::operator=(ModelSupplementScaffold&& moveFrom) never_throws
    {
		assert(!_rawMemoryBlock);		// (not thread safe to use this operator after we've hit "ready" status
        _rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
        _largeBlocksOffset = moveFrom._largeBlocksOffset;
		_chunkFile = std::move(moveFrom._chunkFile);
		return *this;
    }

    ModelSupplementScaffold::~ModelSupplementScaffold()
    {
        ImmutableData().~ModelSupplementImmutableData();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    std::ostream& StreamOperator(std::ostream& stream, const GeoInputAssembly& ia)
    {
        stream << "Stride: " << ia._vertexStride << ": ";
        for (size_t c=0; c<ia._elements.size(); c++) {
            if (c != 0) stream << ", ";
            const auto& e = ia._elements[c];
            stream << e._semanticName << "[" << e._semanticIndex << "] " << AsString(e._nativeFormat);
        }
        return stream;
    }

    std::ostream& StreamOperator(std::ostream& stream, const DrawCallDesc& dc)
    {
        return stream << "Mat: " << dc._subMaterialIndex << ", DrawIndexed(" << dc._indexCount << ", " << dc._firstIndex << ", " << dc._firstVertex << ")";
    }

    GeoInputAssembly CreateGeoInputAssembly(   
        const std::vector<InputElementDesc>& vertexInputLayout,
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


}}
