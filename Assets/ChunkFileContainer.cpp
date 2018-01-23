// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ChunkFileContainer.h"
#include "BlockSerializer.h"
#include "DepVal.h"
#include "IFileSystem.h"
#include "MemoryFile.h"
#include "../Utility/StringFormat.h"
#include "../Core/Exceptions.h"

namespace Assets
{
    std::vector<AssetChunkResult> ChunkFileContainer::ResolveRequests(
        IteratorRange<const AssetChunkRequest*> requests) const
    {
		auto file = OpenFile();
        return ResolveRequests(*file, requests);
    }

	std::shared_ptr<IFileInterface> ChunkFileContainer::OpenFile() const
	{
		std::shared_ptr<IFileInterface> result;
		if (_blob)
			return CreateMemoryFile(_blob);
		return MainFileSystem::OpenFileInterface(_filename.c_str(), "rb");
	}

    std::vector<AssetChunkResult> ChunkFileContainer::ResolveRequests(
        IFileInterface& file, IteratorRange<const AssetChunkRequest*> requests) const
    {
        auto chunks = Serialization::ChunkFile::LoadChunkTable(file);
        
        std::vector<AssetChunkResult> result;
        result.reserve(requests.size());

            // First scan through and check to see if we
            // have all of the chunks we need
        using ChunkHeader = Serialization::ChunkFile::ChunkHeader;
        for (const auto& r:requests) {
            auto i = std::find_if(
                chunks.begin(), chunks.end(), 
                [&r](const ChunkHeader& c) { return c._type == r._type; });
            if (i == chunks.end())
                Throw(Exceptions::ConstructionError(
					Exceptions::ConstructionError::Reason::MissingFile,
					_validationCallback,
                    StringMeld<128>() << "Missing chunk (" << r._name << ")", _filename.c_str()));

            if (i->_chunkVersion != r._expectedVersion)
                Throw(::Assets::Exceptions::ConstructionError(
					Exceptions::ConstructionError::Reason::UnsupportedVersion,
					_validationCallback,
                    StringMeld<256>() 
                        << "Data chunk is incorrect version for chunk (" 
                        << r._name << ") expected: " << r._expectedVersion << ", got: " << i->_chunkVersion, 
						_filename.c_str()));
        }

        for (const auto& r:requests) {
            auto i = std::find_if(
                chunks.begin(), chunks.end(), 
                [&r](const ChunkHeader& c) { return c._type == r._type; });
            assert(i != chunks.end());

            AssetChunkResult chunkResult;
            if (	r._dataType == AssetChunkRequest::DataType::BlockSerializer
				||	r._dataType == AssetChunkRequest::DataType::Raw) {
                uint8* mem = (uint8*)XlMemAlign(i->_size, sizeof(uint64_t));
                chunkResult._buffer = std::unique_ptr<uint8[], PODAlignedDeletor>(mem);
                chunkResult._bufferSize = i->_size;
                file.Seek(i->_fileOffset);
                file.Read(chunkResult._buffer.get(), i->_size);

                // initialize with the block serializer (if requested)
                if (r._dataType == AssetChunkRequest::DataType::BlockSerializer)
                    Serialization::Block_Initialize(chunkResult._buffer.get());
            } else if (r._dataType == AssetChunkRequest::DataType::ReopenFunction) {
				auto offset = i->_fileOffset;
				auto blobCopy = _blob;
				auto filenameCopy = _filename;
				auto depValCopy = _validationCallback;
				chunkResult._reopenFunction = [offset, blobCopy, filenameCopy, depValCopy]() -> std::shared_ptr<IFileInterface> {
					TRY {
						std::shared_ptr<IFileInterface> result;
						if (blobCopy) {
							result = CreateMemoryFile(blobCopy);
						} else 
							result = MainFileSystem::OpenFileInterface(filenameCopy.c_str(), "rb");
						result->Seek(offset);
						return result;
					} CATCH (const std::exception& e) {
						Throw(Exceptions::ConstructionError(e, depValCopy));
					} CATCH_END
				};
			}

            result.emplace_back(std::move(chunkResult));
        }

        return result;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    ChunkFileContainer::ChunkFileContainer(StringSection<ResChar> assetTypeName)
    : _filename(assetTypeName.AsString())
    {
		_validationCallback = std::make_shared<DependencyValidation>();
		RegisterFileDependency(_validationCallback, MakeStringSection(_filename));
    }

	ChunkFileContainer::ChunkFileContainer(const Blob& blob, const DepValPtr& depVal, StringSection<ResChar>)
	: _filename("<<in memory>>")
	, _blob(blob), _validationCallback(depVal)
	{			
	}

	ChunkFileContainer::ChunkFileContainer() {}
    ChunkFileContainer::~ChunkFileContainer() {}

}

