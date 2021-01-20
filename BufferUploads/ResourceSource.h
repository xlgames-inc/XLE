// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IBufferUploads.h"

#include "MemoryManagement.h"
#include "PlatformInterface.h"
#include "ThreadContext.h"
#include "../RenderCore/IDevice_Forward.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/Threading/ThreadingUtils.h"
#include "../Utility/Optional.h"

#define D3D_BUFFER_UPLOAD_USE_WAITABLE_QUEUES

namespace BufferUploads
{
        /////   R E S O U R C E S   P O O L   /////

    typedef uint64 DescHash;

    template <typename Desc> class ResourcesPool : public IResourcePool, public std::enable_shared_from_this<ResourcesPool<Desc>>
    {
    public:
        using UnderlyingResource = RenderCore::IResource;
		using UnderlyingResourcePtr = RenderCore::IResourcePtr;

        intrusive_ptr<ResourceLocator>     CreateResource(const Desc&, unsigned realSize, bool&deviceCreation);

        virtual void AddRef(
            uint64 resourceMarker, UnderlyingResource* resource, 
            unsigned offset, unsigned size);

        virtual void ReturnToPool(
            uint64 resourceMarker, UnderlyingResourcePtr&& resource, 
            unsigned offset, unsigned size);

        std::vector<PoolMetrics>    CalculateMetrics() const;
		RenderCore::IDevice*        GetUnderlyingDevice() { return _underlyingDevice; }
        void                        OnLostDevice();
        void                        Update(unsigned newFrameID);

        ResourcesPool(RenderCore::IDevice& device, unsigned retainFrames = ~unsigned(0x0));
        ~ResourcesPool();
    protected:
        class PoolOfLikeResources
        {
        public:
            auto        AllocateResource(unsigned realSize, bool& deviceCreation) -> UnderlyingResourcePtr;
            const Desc& GetDesc() const { return _desc; }
            PoolMetrics CalculateMetrics() const;
            void        Update(unsigned newFrameID);
            void        ReturnToPool(UnderlyingResourcePtr&& resource);

            PoolOfLikeResources(RenderCore::IDevice& underlyingDevice, const Desc&, unsigned retainFrames = ~unsigned(0x0));
            ~PoolOfLikeResources();
        private:
            struct Entry
            {
                UnderlyingResourcePtr  _underlying;
                unsigned            _returnFrameID;
            };
            LockFreeFixedSizeQueue<Entry, 512> _allocableResources;
            Desc                        _desc;
            mutable unsigned            _peakSize;
            mutable Interlocked::Value  _recentDeviceCreateCount, _recentPoolCreateCount, _recentReleaseCount;
            Interlocked::Value          _totalCreateSize, _totalCreateCount, _totalRealSize;
            unsigned                    _currentFrameID;
            unsigned                    _retainFrames;
			RenderCore::IDevice*        _underlyingDevice;
        };

            //
            //          >   Manual hash table; sorted vector with a     <
            //          >   pointer to the payload                      <
            //
        typedef std::pair<DescHash, std::shared_ptr<PoolOfLikeResources> > HashTableEntry;
        typedef std::vector<HashTableEntry> HashTable;
        HashTable                       _hashTables[2];
        volatile Interlocked::Value     _readerCount[2];
        unsigned                        _hashTableIndex;
        mutable Threading::Mutex        _writerLock;
        unsigned                        _retainFrames;
		RenderCore::IDevice*            _underlyingDevice;

        struct CompareFirst
        {
            bool operator()(const HashTableEntry& lhs, const HashTableEntry& rhs) { return lhs.first < rhs.first; }
            bool operator()(const HashTableEntry& lhs, DescHash rhs) { return lhs.first < rhs; }
            bool operator()(DescHash lhs, const HashTableEntry& rhs) { return lhs < rhs.first; }
        };
    };

        /////   B A T C H E D   R E S O U R C E S   /////

    class BatchedResources : public IResourcePool, public std::enable_shared_from_this<BatchedResources>
    {
    public:
        using UnderlyingResource = RenderCore::IResource;
		using UnderlyingResourcePtr = RenderCore::IResourcePtr;

        intrusive_ptr<ResourceLocator> Allocate(unsigned size, bool& deviceCreation, const char name[]);

        virtual void AddRef(
            uint64 resourceMarker, UnderlyingResource* resource, 
            unsigned offset, unsigned size);

        virtual void ReturnToPool(
            uint64 resourceMarker, UnderlyingResourcePtr&& resource, 
            unsigned offset, unsigned size);

            //
            //      Two step destruction process... Deref to remove the reference first. But if Deref returns
            //      "PerformDeallocate", caller must also call Deallocate. In some cases the caller may not 
            //      want to do the deallocate immediately -- (eg, waiting for GPU, or shifting it into another thread)
            //
        struct ResultFlags { enum Enum { IsBatched = 1<<0, PerformDeallocate = 1<<1, IsCurrentlyDefragging = 1<<2 }; typedef unsigned BitField; };
        ResultFlags::BitField   IsBatchedResource(UnderlyingResource* resource) const;
        ResultFlags::BitField   Validate(const ResourceLocator& locator) const;
        BatchingSystemMetrics   CalculateMetrics() const;
        const BufferDesc&       GetPrototype() const { return _prototype; }

        void                    TickDefrag(ThreadContext& deviceContext, IManager::EventListID processedEventList, bool& deviceCreation);
        void                    OnLostDevice();

        BatchedResources(const BufferDesc& prototype, std::shared_ptr<ResourcesPool<BufferDesc>> sourcePool);
        ~BatchedResources();
    private:
        class HeapedResource
        {
        public:
            unsigned            Allocate(unsigned size, const char name[]);
            void                Allocate(unsigned ptr, unsigned size);
            void                Deallocate(unsigned ptr, unsigned size);

            bool                AddRef(unsigned ptr, unsigned size, const char name[]);
            bool                Deref(unsigned ptr, unsigned size);
            
            BatchedHeapMetrics  CalculateMetrics() const;
            float               CalculateFragmentationWeight() const;
            void                ValidateRefsAndHeap();

            HeapedResource();
            HeapedResource(const BufferDesc& desc, const intrusive_ptr<ResourceLocator>& heapResource);
            ~HeapedResource();

            intrusive_ptr<ResourceLocator> _heapResource;
            SimpleSpanningHeap  _heap;
            ReferenceCountingLayer _refCounts;
            unsigned _size;
            unsigned _defragCount;
            uint64 _hashLastDefrag;
        };

        class ActiveDefrag
        {
        public:
            struct Operation  { enum Enum { Deallocate }; };
            void                QueueOperation(Operation::Enum operation, unsigned start, unsigned end);
            void                ApplyPendingOperations(HeapedResource& destination);

            void                Tick(ThreadContext& context, const UnderlyingResourcePtr& sourceResource);
            bool                IsCompleted(IManager::EventListID processedEventList, ThreadContext& context);

            void                SetSteps(const SimpleSpanningHeap& sourceHeap, const std::vector<DefragStep>& steps);
            void                ReleaseSteps();
            const std::vector<DefragStep>&  GetSteps() { return _steps; }

            HeapedResource*     GetHeap() { return _newHeap.get(); }
            std::unique_ptr<HeapedResource>&&    ReleaseHeap();

            ActiveDefrag();
            ~ActiveDefrag();

        private:
            struct PendingOperation { unsigned _start, _end; Operation::Enum _operation; };
            std::vector<PendingOperation>   _pendingOperations;

            bool                            _doneResourceCopy;
            IManager::EventListID           _eventId;
            std::unique_ptr<HeapedResource>   _newHeap;
            std::vector<DefragStep>         _steps;

            PlatformInterface::GPUEventStack::EventID _initialCommandListID;

            static bool SortByPosition(const PendingOperation& lhs, const PendingOperation& rhs);
        };

        std::vector<std::unique_ptr<HeapedResource>> _heaps;
        BufferDesc _prototype;
        std::shared_ptr<ResourcesPool<BufferDesc>> _sourcePool;
        mutable Threading::ReadWriteMutex _lock;

            //  Active defrag stuff...
        std::unique_ptr<ActiveDefrag> _activeDefrag;
        Threading::Mutex _activeDefrag_Lock;
        HeapedResource* _activeDefragHeap;

        intrusive_ptr<ResourceLocator> _temporaryCopyBuffer;
        unsigned _temporaryCopyBufferCountDown;

        BatchedResources(const BatchedResources&);
        BatchedResources& operator=(const BatchedResources&);
    };

        /////   R E S O U R C E   S O U R C E   /////

    class ResourceSource
    {
    public:
        struct ResourceConstruction
        {
            struct Flags 
            { 
                enum Enum {
                    InitialisationSuccessful     = 1<<0,
                    DelayForBatching             = 1<<1,
                    DeviceConstructionInvoked    = 1<<2
                };
                typedef unsigned BitField;
            };
            intrusive_ptr<ResourceLocator> _identifier;
            Flags::BitField _flags;
            ResourceConstruction() : _flags(0) {}
        };

        struct CreationOptions
        {
            enum Enum {
                AllowDeviceCreation = 1<<0
            };
            typedef unsigned BitField;
        };

        ResourceConstruction    Create(const BufferDesc& desc, DataPacket* initialisationData=NULL, CreationOptions::BitField options=CreationOptions::AllowDeviceCreation);
        void                    Validate(const ResourceLocator& locator);

        BatchedResources&       GetBatchedResources()             { return *_batchedIndexBuffers; }
        PoolSystemMetrics       CalculatePoolMetrics() const;
        void                    Tick(ThreadContext& context, IManager::EventListID processedEventList, bool& deviceCreation);

        bool                    WillBeBatched(const BufferDesc& desc);
        BatchedResources::ResultFlags::BitField     IsBatchedResource(const ResourceLocator& locator, const BufferDesc& desc);

        void                    GetQueueEvents(XlHandle waitEvents[], unsigned& waitEventsCount);

        void                    FlushDelayedReleases(unsigned gpuBarrierProgress=~unsigned(0x0), bool duringDestructor=false);
        bool                    MarkBarrier(unsigned barrierID);

        void                    OnLostDevice();

        ResourceSource(RenderCore::IDevice& device);
        ~ResourceSource();

    protected:
        std::shared_ptr<ResourcesPool<BufferDesc>>      _stagingBufferPool;
        std::shared_ptr<ResourcesPool<BufferDesc>>      _pooledGeometryBuffers;
        std::shared_ptr<BatchedResources>   _batchedIndexBuffers;
		std::optional<Threading::ThreadId>	_flushThread;
        unsigned                            _frameID;
        Threading::Mutex                    _flushDelayedReleasesLock;
		RenderCore::IDevice*                _underlyingDevice;

        #if defined(D3D_BUFFER_UPLOAD_USE_WAITABLE_QUEUES)
            LockFreeFixedSizeQueue_Waitable<intrusive_ptr<ResourceLocator>,256> _delayedReleases;
        #else
            LockFreeFixedSizeQueue<intrusive_ptr<ResourceLocator>,256> _delayedReleases;
        #endif

        inline bool UsePooling(const BufferDesc& input)     { return (input._type == BufferDesc::Type::LinearBuffer) && (input._linearBufferDesc._sizeInBytes < (32*1024)) && (input._allocationRules & AllocationRules::Pooled); }
        inline bool UseBatching(const BufferDesc& input)    { return !!(input._allocationRules & AllocationRules::Batched); }
        BufferDesc AdjustDescForReusableResource(const BufferDesc& input);
    };

}
