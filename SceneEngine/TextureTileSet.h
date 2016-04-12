// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../BufferUploads/IBufferUploads.h"
#include "../../RenderCore/Metal/Forward.h"
#include "../../RenderCore/Metal/TextureView.h"
#include "../../Math/Vector.h"
#include "../../Utility/BitHeap.h"
#include "../../Utility/HeapUtils.h"
#include <assert.h>

namespace SceneEngine
{
    class TextureTile;

    /// <summary>A set of "texture tiles", all of which are the same size</summary>
    /// Used by the terrain rendering to manage and stream various texture resourecs
    class TextureTileSet
    {
    public:
        void    Transaction_Begin(
            TextureTile& tile,
            const void* fileHandle, size_t offset, size_t dataSize);

        bool    IsValid(const TextureTile& tile) const;

        auto    GetBufferUploads() -> BufferUploads::IManager&                      { return *_bufferUploads; }
        auto    GetShaderResource() -> RenderCore::Metal::ShaderResourceView&       { return _shaderResource; }
        auto    GetUnorderedAccessView() -> RenderCore::Metal::UnorderedAccessView& { assert(_uav.GetUnderlying()); return _uav; }

        Int2    GetTileSize() const { return _elementSize; }
        auto    GetFormat() const -> RenderCore::Metal::NativeFormat::Enum { return _format; }
        void    SetPriorityMode(bool priorityMode);
        bool    GetPriorityMode() const { return _priorityMode; }
        
        TextureTileSet(
            BufferUploads::IManager& bufferUploads,
            Int2 elementSize, unsigned elementCount,
            RenderCore::Metal::NativeFormat::Enum format,
            bool allowModification);
        ~TextureTileSet();

    private:
        class ArraySlice
        {
        public:
            BitHeap     _allocationFlags;
            unsigned    _unallocatedCount;

            ArraySlice(int count);
            ArraySlice(ArraySlice&& moveFrom);
            ArraySlice& operator=(ArraySlice&& moveFrom);
        };

        Int2                        _elementsPerArraySlice;
        Int2                        _elementSize;
        std::vector<ArraySlice>     _slices;
        BufferUploads::IManager *   _bufferUploads;
        bool                        _allowModification;
        bool                        _priorityMode;

        std::vector<unsigned>       _uploadIds;
        mutable LRUQueue            _lruQueue;

        intrusive_ptr<BufferUploads::ResourceLocator>   _resource;
        RenderCore::Metal::ShaderResourceView           _shaderResource;
        RenderCore::Metal::UnorderedAccessView          _uav;
        BufferUploads::TransactionID                    _creationTransaction;

        RenderCore::Metal::NativeFormat::Enum _format;

        void    CompleteCreation();

        TextureTileSet(const TextureTileSet& cloneFrom) = delete;
        TextureTileSet& operator=(const TextureTileSet& cloneFrom) = delete;
    };

    class TextureTile
    {
    public:
        BufferUploads::TransactionID _transaction;
        unsigned _x, _y, _arrayIndex;
        unsigned _width, _height;
        unsigned _uploadId;

        TextureTile();
        ~TextureTile();
        TextureTile(TextureTile&& moveFrom);
        TextureTile& operator=(TextureTile&& moveFrom);

        void swap(TextureTile& other);
    };
}