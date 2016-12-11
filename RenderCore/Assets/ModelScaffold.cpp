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

	void ModelScaffold::Resolve() const
	{
		if (_deferredConstructor) {
			auto state = _deferredConstructor->GetAssetState();
			if (state == ::Assets::AssetState::Pending)
				Throw(::Assets::Exceptions::PendingAsset(_filename.c_str(), "Pending deferred construction"));

			auto constructor = std::move(_deferredConstructor);
			if (state == ::Assets::AssetState::Ready) {
				*const_cast<ModelScaffold*>(this) = std::move(*constructor->PerformConstructor<ModelScaffold>());
			} else {
				assert(state == ::Assets::AssetState::Invalid);
			}
		}
		if (!_rawMemoryBlock)
			Throw(::Assets::Exceptions::InvalidAsset(_filename.c_str(), "Missing data"));
	}

	::Assets::AssetState ModelScaffold::TryResolve() const
	{
		if (_deferredConstructor) {
			auto state = _deferredConstructor->GetAssetState();
			if (state == ::Assets::AssetState::Pending)
				return state;

			auto constructor = std::move(_deferredConstructor);
			if (state == ::Assets::AssetState::Ready) {
				*const_cast<ModelScaffold*>(this) = std::move(*constructor->PerformConstructor<ModelScaffold>());
			} // (else fall through);
		}

		return _rawMemoryBlock ? ::Assets::AssetState::Ready : ::Assets::AssetState::Invalid;
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
    
    ModelScaffold::ModelScaffold(const ::Assets::ChunkFileContainer& chunkFile)
	: _filename(chunkFile.Filename())
	, _depVal(chunkFile.GetDependencyValidation())
    {
        auto chunks = chunkFile.ResolveRequests(MakeIteratorRange(ModelScaffoldChunkRequests));
		assert(chunks.size() == 2);
		_rawMemoryBlock = std::move(chunks[0]._buffer);
		_largeBlocksOffset = chunks[1]._offset;
    }

    ModelScaffold::ModelScaffold(const std::shared_ptr<::Assets::DeferredConstruction>& deferredConstruction)
    : _deferredConstructor(deferredConstruction)
	, _depVal(deferredConstruction->GetDependencyValidation())
    {}

    ModelScaffold::ModelScaffold(ModelScaffold&& moveFrom) never_throws
    : _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
    , _largeBlocksOffset(moveFrom._largeBlocksOffset)
	, _deferredConstructor(std::move(moveFrom._deferredConstructor))
	, _filename(std::move(moveFrom._filename))
	, _depVal(std::move(moveFrom._depVal))
    {}

    ModelScaffold& ModelScaffold::operator=(ModelScaffold&& moveFrom) never_throws
    {
        _rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
        _largeBlocksOffset = moveFrom._largeBlocksOffset;
		_deferredConstructor = std::move(moveFrom._deferredConstructor);
		_filename = std::move(moveFrom._filename);
		_depVal = std::move(moveFrom._depVal);
        return *this;
    }

    ModelScaffold::~ModelScaffold()
    {
        auto* data = TryImmutableData();
        if (data)
            data->~ModelImmutableData();
    }

	std::shared_ptr<::Assets::DeferredConstruction> ModelScaffold::BeginDeferredConstruction(
		const ::Assets::ResChar* initializers[], unsigned initializerCount)
	{
		return ::Assets::DefaultBeginDeferredConstruction<ModelScaffold>(initializers, initializerCount);
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

	void ModelSupplementScaffold::Resolve() const
	{
		if (_deferredConstructor) {
			auto state = _deferredConstructor->GetAssetState();
			if (state == ::Assets::AssetState::Pending)
				Throw(::Assets::Exceptions::PendingAsset(_filename.c_str(), "Pending deferred construction"));

			auto constructor = std::move(_deferredConstructor);
			if (state == ::Assets::AssetState::Ready) {
				*const_cast<ModelSupplementScaffold*>(this) = std::move(*constructor->PerformConstructor<ModelSupplementScaffold>());
			} else {
				assert(state == ::Assets::AssetState::Invalid);
			}
		}
		if (!_rawMemoryBlock)
			Throw(::Assets::Exceptions::InvalidAsset(_filename.c_str(), "Missing data"));
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
    
    ModelSupplementScaffold::ModelSupplementScaffold(const ::Assets::ChunkFileContainer& chunkFile)
	: _filename(chunkFile.Filename())
	, _depVal(chunkFile.GetDependencyValidation())
	{
		auto chunks = chunkFile.ResolveRequests(MakeIteratorRange(ModelSupplementScaffoldChunkRequests));
		assert(chunks.size() == 2);
		_rawMemoryBlock = std::move(chunks[0]._buffer);
		_largeBlocksOffset = chunks[1]._offset;
	}

    ModelSupplementScaffold::ModelSupplementScaffold(const std::shared_ptr<::Assets::DeferredConstruction>& deferredConstruction)
	: _deferredConstructor(deferredConstruction)
	, _depVal(deferredConstruction->GetDependencyValidation())
	{}

    ModelSupplementScaffold::ModelSupplementScaffold(ModelSupplementScaffold&& moveFrom)
    : _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
    , _largeBlocksOffset(moveFrom._largeBlocksOffset)
	, _deferredConstructor(std::move(moveFrom._deferredConstructor))
	, _filename(std::move(moveFrom._filename))
	, _depVal(moveFrom._depVal)
    {}

    ModelSupplementScaffold& ModelSupplementScaffold::operator=(ModelSupplementScaffold&& moveFrom)
    {
        _rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
        _largeBlocksOffset = moveFrom._largeBlocksOffset;
		_deferredConstructor = std::move(moveFrom._deferredConstructor);
		_filename = std::move(moveFrom._filename);
		_depVal = std::move(moveFrom._depVal);
        return *this;
    }

    ModelSupplementScaffold::~ModelSupplementScaffold()
    {
        auto* data = TryImmutableData();
        if (data)
            data->~ModelSupplementImmutableData();
    }

	std::shared_ptr<::Assets::DeferredConstruction> ModelSupplementScaffold::BeginDeferredConstruction(
		const ::Assets::ResChar* initializers[], unsigned initializerCount)
	{
		// Special case version of this function for ModelSupplementScaffold
		// First parameter is actually a uint64, which is our compile type
		assert(initializerCount >= 2);
		return ::Assets::DefaultBeginDeferredConstruction<ModelScaffold>(initializers+1, initializerCount-1, *(const uint64*)initializers[0]);
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
