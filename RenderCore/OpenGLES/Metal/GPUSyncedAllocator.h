// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Buffer.h"
#include "../../../Utility/IteratorUtils.h"
#include <memory>
#include <vector>

namespace RenderCore { namespace Metal_OpenGLES
{
    class DeviceContext;
    class SyncEventSet;
    class Buffer;

    class GPUSyncedAllocator
    {
    public:
        unsigned Allocate(unsigned size);
        unsigned GetTotalSize() const { return _totalSize; }
        void Stall();
        void SetGPUMarker();

        GPUSyncedAllocator(unsigned totalSize);
        ~GPUSyncedAllocator();
    private:
        struct Allocation { unsigned _start, _end, _syncPoint; };
        std::vector<Allocation>             _allocationsBehind;
        std::vector<Allocation>             _allocationsInfront;
        std::unique_ptr<SyncEventSet>       _eventSet;

        unsigned _movingPoint, _totalSize;
    };

    class DynamicBuffer
    {
    public:
        unsigned Write(DeviceContext& devContext, IteratorRange<const void*>);
        Buffer& GetBuffer()             { return _underlyingBuffer; }
        size_t GetResourceSize() const  { return _resourceSize; }

        DynamicBuffer(const RenderCore::LinearBufferDesc& desc, RenderCore::BindFlag::BitField bindFlags, StringSection<> name, bool useSyncInterface);
        ~DynamicBuffer();
    private:
        Buffer                                  _underlyingBuffer;
        std::unique_ptr<GPUSyncedAllocator>     _syncedAllocator;
        size_t                                  _resourceSize;

        unsigned _allocatedSizeSinceLastMarker;
    };
}}

