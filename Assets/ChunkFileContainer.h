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

namespace Assets
{
    class PendingCompileMarker;
    class DependencyValidation;
    class ICompileMarker;

    class AssetChunkRequest
    {
    public:
        const char*     _name;
        Serialization::ChunkFile::TypeIdentifier _type;
        unsigned        _expectedVersion;
        
        enum class DataType
        {
            DontLoad, Raw, BlockSerializer
        };
        DataType        _dataType;
    };

    class AssetChunkResult
    {
    public:
        Serialization::ChunkFile::SizeType  _offset;
        std::unique_ptr<uint8[], PODAlignedDeletor> _buffer;
        size_t _size;

        AssetChunkResult() : _offset(0), _size(0) {}
        AssetChunkResult(AssetChunkResult&& moveFrom)
        : _offset(moveFrom._offset)
        , _buffer(std::move(moveFrom._buffer))
        , _size(moveFrom._size)
        {}
        AssetChunkResult& operator=(AssetChunkResult&& moveFrom)
        {
            _offset = moveFrom._offset;
            _buffer = std::move(moveFrom._buffer);
            _size = moveFrom._size;
            return *this;
        }
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

		ChunkFileContainer(StringSection<ResChar> assetTypeName);
        ~ChunkFileContainer();

		ChunkFileContainer(const ChunkFileContainer&) = delete;
		ChunkFileContainer& operator=(const ChunkFileContainer&) = delete;
    private:
        rstring			_filename;
		DepValPtr		_validationCallback;
    };

}



