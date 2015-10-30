// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "ChunkFile.h"
#include "../Utility/IteratorUtils.h"

namespace Assets
{
    class PendingCompileMarker;
    class DependencyValidation;

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
        std::unique_ptr<uint8[]> _buffer;
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
    class ChunkFileAsset
    {
    public:
        const rstring& Filename() const       { return _filename; }
         
        auto GetDependencyValidation() const -> const std::shared_ptr<DependencyValidation>& { return _validationCallback; }

        void Resolve() const;
        ::Assets::AssetState TryResolve() const;
        ::Assets::AssetState StallAndResolve() const;

        ChunkFileAsset(ChunkFileAsset&& moveFrom) never_throws;
        ChunkFileAsset& operator=(ChunkFileAsset&& moveFrom) never_throws;
        ~ChunkFileAsset();

    protected:
        using ResolveFn = void(void*, IteratorRange<AssetChunkResult*>);

        ChunkFileAsset(const char assetTypeName[]);
        void Prepare(
            const ::Assets::ResChar filename[], 
            IteratorRange<const AssetChunkRequest*> requests,
            ResolveFn* resolveFn);
        void Prepare(
            std::shared_ptr<PendingCompileMarker>&& marker,
            IteratorRange<const AssetChunkRequest*> requests,
            ResolveFn* resolveFn);
    private:
        rstring                                         _filename;
        mutable std::shared_ptr<PendingCompileMarker>   _marker;
        std::shared_ptr<DependencyValidation>           _validationCallback;

        IteratorRange<const AssetChunkRequest*>     _requests;
        ResolveFn*                                  _resolveFn;

        const char* _assetTypeName;

        void CompleteFromMarker(::Assets::PendingCompileMarker& marker);
        static void ExecuteResolve(
            ResolveFn*, void*, IteratorRange<AssetChunkResult*>, 
            const ResChar filename[], const char assetNameType[]);
    };

}



