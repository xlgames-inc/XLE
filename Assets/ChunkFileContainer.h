// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "ChunkFile.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/MemoryUtils.h"
#include <functional>
#include <memory>

namespace Assets
{
    class CompileFuture;
    class DependencyValidation;
    class ICompileMarker;

	using AssetChunkReopenFunction = std::function<std::shared_ptr<IFileInterface>()>;

    class AssetChunkRequest
    {
    public:
		const char*		_name;		// for debugging purposes, to make it easier to track requests
        Serialization::ChunkFile::TypeIdentifier _type;
        unsigned        _expectedVersion;
        
        enum class DataType
        {
            ReopenFunction, Raw, BlockSerializer
        };
        DataType        _dataType;
    };

    class AssetChunkResult
    {
    public:
        std::unique_ptr<uint8[], PODAlignedDeletor> _buffer;
        size_t                                      _bufferSize = 0;
		AssetChunkReopenFunction					_reopenFunction;
    };

    /// <summary>Utility for building asset objects that load from chunk files (sometimes asychronously)</summary>
    /// Some simple assets simply want to load some raw data from a chunk in a file, or
    /// perhaps from a few chunks in the same file. This is a base class to take away some
    /// of the leg-work involved in implementing that class.
    class ChunkFileContainer
    {
    public:
        const rstring& Filename() const						{ return _filename; }
		const DepValPtr& GetDependencyValidation() const	{ return _validationCallback; }

		std::vector<AssetChunkResult> ResolveRequests(IteratorRange<const AssetChunkRequest*> requests) const;
        std::vector<AssetChunkResult> ResolveRequests(IFileInterface& file, IteratorRange<const AssetChunkRequest*> requests) const;

		std::shared_ptr<IFileInterface> OpenFile() const;

		ChunkFileContainer(StringSection<ResChar> assetTypeName);
		ChunkFileContainer(const Blob& blob, const DepValPtr& depVal, StringSection<ResChar>);
		ChunkFileContainer();
        ~ChunkFileContainer();

		ChunkFileContainer(const ChunkFileContainer&) = default;
		ChunkFileContainer& operator=(const ChunkFileContainer&) = default;
		ChunkFileContainer(ChunkFileContainer&&) never_throws = default;
		ChunkFileContainer& operator=(ChunkFileContainer&&) never_throws = default;
    private:
        rstring			_filename;
		Blob			_blob;
		DepValPtr		_validationCallback;
    };

}



