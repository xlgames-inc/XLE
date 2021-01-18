// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GPUSyncedAllocator.h"
#include "QueryPool.h"
#include "DeviceContext.h"
#include "../../../OSServices/Log.h"
#include "../../../Utility/IteratorUtils.h"
#include "../../../Utility/BitUtils.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    unsigned GPUSyncedAllocator::Allocate(unsigned size, unsigned alignment)
    {
        unsigned crashPoint = _totalSize;
        if (!_allocationsInfront.empty())
            crashPoint = _allocationsInfront[0]._start;

        unsigned preBufferForAlignment = CeilToMultiple(_movingPoint, alignment) - _movingPoint;
        auto headRoom = crashPoint - _movingPoint;
        if (headRoom < (size+preBufferForAlignment)) {
            // If we run out of space, check to see how many allocations we can just write over
            auto lastCompleted = _eventSet->LastCompletedEvent();
            while (!_allocationsInfront.empty() && _allocationsInfront.begin()->_syncPoint <= lastCompleted)
                _allocationsInfront.erase(_allocationsInfront.begin());
            crashPoint = _allocationsInfront.empty() ? _totalSize : _allocationsInfront[0]._start;
            headRoom = crashPoint - _movingPoint;

            if (_allocationsInfront.empty()) {
                // try to erase from _allocationsBehind, also
                while (!_allocationsBehind.empty() && _allocationsBehind.begin()->_syncPoint <= lastCompleted)
                    _allocationsBehind.erase(_allocationsBehind.begin());
                crashPoint = _totalSize;
                headRoom = crashPoint - _movingPoint;

                // we can now choose to reset "_movingPoint" back to the start. But we should only
                // do this if we still don't have enough room for the allocation
                if (headRoom < (size+preBufferForAlignment)) {
                    if (!_allocationsBehind.empty()) {
                        _allocationsInfront = std::move(_allocationsBehind);
                        assert(_allocationsBehind.empty()); // Expecting all 'behind' allocations to be moved into 'infront'
                        _movingPoint = 0;
                        preBufferForAlignment = 0;      // always zero, since _movingPoint is zero
                        headRoom = _allocationsInfront[0]._start - _movingPoint;
                    } else {
                        _movingPoint = 0;
                        preBufferForAlignment = 0;      // always zero, since _movingPoint is zero
                        headRoom = _totalSize - _movingPoint;
                    }
                }
            }
        }

        if (headRoom < (size+preBufferForAlignment)) return ~0u;

        auto result = _movingPoint;
        auto nextEvent = _eventSet->NextEventToSet();
        // just merge into the previous allocation marker, if we can
        if (!_allocationsBehind.empty() && (_allocationsBehind.end()-1)->_syncPoint == nextEvent) {
            (_allocationsBehind.end()-1)->_end = result+size+preBufferForAlignment;
        } else {
            _allocationsBehind.push_back(Allocation {result, result+size+preBufferForAlignment, nextEvent});
        }
        _movingPoint += size+preBufferForAlignment;
        return result+preBufferForAlignment;
    }

    void GPUSyncedAllocator::SetGPUMarker()
    {
        _eventSet->SetEvent();
    }

    GPUSyncedAllocator::GPUSyncedAllocator(IThreadContext *context, unsigned totalSize)
    : _movingPoint(0), _totalSize(totalSize)
    {
        _eventSet = std::make_unique<SyncEventSet>(context);
    }

    GPUSyncedAllocator::~GPUSyncedAllocator() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    unsigned DynamicBuffer::Write(DeviceContext& devContext, IteratorRange<const void*> data)
    {
        assert(data.size() <= _resourceSize); // DynamicBuffer failed allocation request because the buffer is too small to fit the entire allocation at once
        if (data.size() > _resourceSize) return ~0u;

        if (_syncedAllocator) {
            // Note that the alignment overhead can cause wierd issues with this "_allocatedSizeSinceLastMarker"
            // record. Even if we've allocated this quantity of bytes from the buffer, we may actually be using
            // significantly more space, due to bytes lost to alignment. It would be better to track both the
            // allocated size and the alignment overhead together.
            if ((_allocatedSizeSinceLastMarker + data.size()) > 8 * 1024) {
                _syncedAllocator->SetGPUMarker();
                _allocatedSizeSinceLastMarker = 0;
            }
            _allocatedSizeSinceLastMarker += (unsigned)data.size();

            // In this path, we monitor the progress on the GPU manually, so we know where the GPU is currently
            // reading from, or is scheduled to read from in the future. The buffer is allocated in a circular
            // fashion, and we write to it using the unsynchronized write flag (which tells the driver that it's
            // safe to write to this location immediately)
            auto offset = _syncedAllocator->Allocate((unsigned)data.size(), _allocationAlignment);
            if (offset == ~0u) {
                Log(Warning) << "Performance warning --- synchronizing CPU/GPU due to (GL-specific) DynamicBuffer allocation failure" << std::endl;
                devContext.GetDevice()->Stall();
                offset = _syncedAllocator->Allocate((unsigned)data.size(), _allocationAlignment);
            }
            if (offset == ~0u)
                return ~0;

            assert((offset+data.size()) <= _resourceSize); // Synced allocator returned allocation outside of valid range
            assert((offset%_allocationAlignment) == 0);
            _underlyingBuffer.Update(
                devContext,
                data.begin(), data.size(), offset,
                Buffer::UpdateFlags::UnsynchronizedWrite);

            return offset;
        } else {
            // In this path, we're not manually managing the allocations. We just use the "dynamic" style underlying
            // buffers
            _underlyingBuffer.Update(devContext, data.begin(), data.size());
            return 0;
        }
    }

    DynamicBuffer::DynamicBuffer(const LinearBufferDesc& desc, BindFlag::BitField bindFlags, StringSection<> name, bool useSyncInterface, IThreadContext *context)
    {
        using namespace RenderCore;
        auto useSyncMarker = useSyncInterface && SyncEventSet::IsSupported();

        auto resourceDesc = CreateDesc(
            bindFlags,
            useSyncMarker ? CPUAccess::Write : CPUAccess::WriteDynamic,
            GPUAccess::Read,
            desc, name);

        _underlyingBuffer = Buffer(GetObjectFactory(), resourceDesc, {});
        assert(_underlyingBuffer.IsGood());
        _resourceSize = desc._sizeInBytes;

        if (useSyncMarker)
            _syncedAllocator = std::make_unique<GPUSyncedAllocator>(context, _resourceSize);

        _allocationAlignment = 1;

        #if defined(GL_ES_VERSION_3_0)
            if (bindFlags & BindFlag::ConstantBuffer) {
                // Some drivers may impose an alignment restriction on us. We must ensure that
                // the start offset of each allocation is a multiple of the given alignment; otherwise
                // we will get GL errors
                GLint offsetAlignment = 0;
                glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &offsetAlignment);
                _allocationAlignment = (unsigned)std::max(1, offsetAlignment);        // (ensure that we don't end up with a value of 0)
            }
        #endif
    }

    DynamicBuffer::~DynamicBuffer() {}
}}

