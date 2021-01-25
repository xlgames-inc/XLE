// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelScaffold.h"
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

    uint64_t GeoInputAssembly::BuildHash() const
    {
            //  Build a hash for this object.
            //  Note that we should be careful that we don't get an
            //  noise from characters in the left-over space in the
            //  semantic names. Do to this right, we should make sure
            //  that left over space has no effect.
        auto elementsHash = Hash64(AsPointer(_elements.cbegin()), AsPointer(_elements.cend()));
        elementsHash ^= uint64_t(_vertexStride);
        return elementsHash;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	const ModelImmutableData&   ModelScaffold::ImmutableData() const                
    {
        return *(const ModelImmutableData*)::Assets::Block_GetFirstObject(_rawMemoryBlock.get());
    }

    const ModelCommandStream&       ModelScaffold::CommandStream() const                { return ImmutableData()._visualScene; }
    const SkeletonMachine&			ModelScaffold::EmbeddedSkeleton() const             { return ImmutableData()._embeddedSkeleton; }
    std::pair<Float3, Float3>       ModelScaffold::GetStaticBoundingBox(unsigned) const { return ImmutableData()._boundingBox; }
    unsigned                        ModelScaffold::GetMaxLOD() const                    { return ImmutableData()._maxLOD; }

	std::shared_ptr<::Assets::IFileInterface>	ModelScaffold::OpenLargeBlocks() const { return _largeBlocksReopen(); }

    const ::Assets::ArtifactRequest ModelScaffold::ChunkRequests[2]
    {
        ::Assets::ArtifactRequest { "Scaffold", ChunkType_ModelScaffold, ModelScaffoldVersion, ::Assets::ArtifactRequest::DataType::BlockSerializer },
        ::Assets::ArtifactRequest { "LargeBlocks", ChunkType_ModelScaffoldLargeBlocks, ModelScaffoldLargeBlocksVersion, ::Assets::ArtifactRequest::DataType::ReopenFunction }
    };
    
    ModelScaffold::ModelScaffold(IteratorRange<::Assets::ArtifactRequestResult*> chunks, const ::Assets::DepValPtr& depVal)
    {
		assert(chunks.size() == 2);
		_rawMemoryBlock = std::move(chunks[0]._buffer);
		_largeBlocksReopen = std::move(chunks[1]._reopenFunction);
		_depVal = depVal;
    }

    ModelScaffold::ModelScaffold(ModelScaffold&& moveFrom) never_throws
    : _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
    , _largeBlocksReopen(std::move(moveFrom._largeBlocksReopen))
	, _depVal(std::move(moveFrom._depVal))
    {}

    ModelScaffold& ModelScaffold::operator=(ModelScaffold&& moveFrom) never_throws
    {
		ImmutableData().~ModelImmutableData();
        _rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
		_largeBlocksReopen = std::move(moveFrom._largeBlocksReopen);
		_depVal = std::move(moveFrom._depVal);
        return *this;
    }

    ModelScaffold::~ModelScaffold()
    {
        ImmutableData().~ModelImmutableData();
    }

//////////////////////////////////////////////////////////////////////////////////////////////////

    const ModelSupplementImmutableData&   ModelSupplementScaffold::ImmutableData() const                
    {
        return *(const ModelSupplementImmutableData*)::Assets::Block_GetFirstObject(_rawMemoryBlock.get());
    }

	std::shared_ptr<::Assets::IFileInterface>	ModelSupplementScaffold::OpenLargeBlocks() const { return _largeBlocksReopen(); }

    const ::Assets::ArtifactRequest ModelSupplementScaffold::ChunkRequests[]
    {
        ::Assets::ArtifactRequest { "Scaffold", ChunkType_ModelScaffold, 0, ::Assets::ArtifactRequest::DataType::BlockSerializer },
        ::Assets::ArtifactRequest { "LargeBlocks", ChunkType_ModelScaffoldLargeBlocks, 0, ::Assets::ArtifactRequest::DataType::ReopenFunction }
    };
    
    ModelSupplementScaffold::ModelSupplementScaffold(IteratorRange<::Assets::ArtifactRequestResult*> chunks, const ::Assets::DepValPtr& depVal)
	: _depVal(depVal)
	{
		assert(chunks.size() == 2);
		_rawMemoryBlock = std::move(chunks[0]._buffer);
		_largeBlocksReopen = chunks[1]._reopenFunction;
	}

    ModelSupplementScaffold::ModelSupplementScaffold(ModelSupplementScaffold&& moveFrom) never_throws
    : _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
    , _largeBlocksReopen(std::move(moveFrom._largeBlocksReopen))
	, _depVal(std::move(moveFrom._depVal))
    {}

    ModelSupplementScaffold& ModelSupplementScaffold::operator=(ModelSupplementScaffold&& moveFrom) never_throws
    {
		ImmutableData().~ModelSupplementImmutableData();
        _rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
		_largeBlocksReopen = std::move(moveFrom._largeBlocksReopen);
		_depVal = std::move(moveFrom._depVal);
		return *this;
    }

    ModelSupplementScaffold::~ModelSupplementScaffold()
    {
        ImmutableData().~ModelSupplementImmutableData();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    std::ostream& SerializationOperator(std::ostream& stream, const GeoInputAssembly& ia)
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

	unsigned BuildLowLevelInputAssembly(
        IteratorRange<InputElementDesc*> dst,
        IteratorRange<const RenderCore::Assets::VertexElement*> source,
        unsigned lowLevelSlot)
    {
        unsigned vertexElementCount = 0;
        for (unsigned i=0; i<source.size(); ++i) {
            auto& sourceElement = source[i];
            assert((vertexElementCount+1) <= dst.size());
            if ((vertexElementCount+1) <= dst.size()) {
                    // in some cases we need multiple "slots". When we have multiple slots, the vertex data 
                    //  should be one after another in the vb (that is, not interleaved)
                dst[vertexElementCount++] = InputElementDesc(
                    sourceElement._semanticName, sourceElement._semanticIndex,
                    sourceElement._nativeFormat, lowLevelSlot, sourceElement._alignedByteOffset);
            }
        }
        return vertexElementCount;
    }

	std::vector<MiniInputElementDesc> BuildLowLevelInputAssembly(IteratorRange<const RenderCore::Assets::VertexElement*> source)
	{
		std::vector<MiniInputElementDesc> result;
		result.reserve(source.size());
		for (unsigned i=0; i<source.size(); ++i) {
            auto& sourceElement = source[i];
			#if defined(_DEBUG)
				auto expectedOffset = CalculateVertexStride(MakeIteratorRange(result), false);
				assert(expectedOffset == sourceElement._alignedByteOffset);
			#endif
			result.push_back(
				MiniInputElementDesc{Hash64(sourceElement._semanticName) + sourceElement._semanticIndex, sourceElement._nativeFormat});
		}
		return result;
	}
}}
