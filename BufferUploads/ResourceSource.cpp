// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ResourceSource.h"
#include "../RenderCore/ResourceUtils.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/BitUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/PtrUtils.h"
#include <algorithm>

#pragma warning(disable:4127) // conditional expression is constant
#pragma warning(disable:4018) // signed/unsigned mismatch
#pragma warning(disable:4389) // signed/unsigned mismatch

namespace BufferUploads
{
    namespace WriteMode { enum Enum { UpdateSubresource, Map }; }

    // ~~~~~~~~~~~~ // ~~~~~~<   >~~~~~~ // ~~~~~~~~~~~~ //

//    #if !defined(XL_RELEASE)    // perhaps just XL_DEBUG? Adds a thread local storage variable
//        #define REUSABLE_RESOURCE_DEBUGGING
//    #endif

    #if defined(REUSABLE_RESOURCE_DEBUGGING)

        // #define THREAD_LOCAL_STORAGE __declspec( thread )

        /*THREAD_LOCAL_STORAGE*/ Interlocked::Value g_oldToDeleteReusableResources = 0;

        class ReusableResourceDestructionHelper : public IUnknown
        {
        public:
            ReusableResourceDestructionHelper(const BufferDesc& desc);
            virtual ~ReusableResourceDestructionHelper();

            virtual HRESULT STDMETHODCALLTYPE   QueryInterface(REFIID riid, __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject);
            virtual ULONG STDMETHODCALLTYPE     AddRef();
            virtual ULONG STDMETHODCALLTYPE     Release();
        private:
            BufferDesc          _desc;
            Interlocked::Value  _referenceCount;
        };

        // {2FB89B77-3946-48DE-8C78-09CF3DCE8651}
        EXTERN_C const GUID DECLSPEC_SELECTANY GUID_ReusableResourceDestructionHelper = { 0x2fb89b77, 0x3946, 0x48de, { 0x8c, 0x78, 0x9, 0xcf, 0x3d, 0xce, 0x86, 0x51 } };

        ReusableResourceDestructionHelper::ReusableResourceDestructionHelper(const BufferDesc& desc) : _desc(desc), _referenceCount(0) {}
        ReusableResourceDestructionHelper::~ReusableResourceDestructionHelper()
        {
            if (!Interlocked::Load(&g_oldToDeleteReusableResources)) {
                LogWarningF("Warning -- destroying reusable resource at unexpected time");
            }
        }

        HRESULT STDMETHODCALLTYPE ReusableResourceDestructionHelper::QueryInterface(REFIID riid, __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject)
        {
            ppvObject = NULL;
            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE ReusableResourceDestructionHelper::AddRef()
        {
            return Interlocked::Increment(&_referenceCount) + 1;
        }

        ULONG STDMETHODCALLTYPE ReusableResourceDestructionHelper::Release()
        {
            Interlocked::Value newRefCount = Interlocked::Decrement(&_referenceCount) - 1;
            if (!newRefCount) {
                delete this;
            }
            return newRefCount;
        }

    #endif

    static DescHash Hash(const TextureDesc& desc)
    {
        DescHash result = 0;
        if (IsPowerOfTwo(desc._width) && IsPowerOfTwo(desc._height)
            && desc._width >= 64 && desc._width <= 16384
            && desc._height >= 64 && desc._height <= 16384) {
            result |= 0x1;  // set the bottom "type" bit
            result |= (unsigned(desc._dimensionality) & 0x1) << 1;
            result |= desc._arrayCount << 2;
            unsigned int widthPower = IntegerLog2(desc._width)-6;
            unsigned int heightPower = IntegerLog2(desc._height)-6;
            result |= widthPower << 8;
            result |= heightPower << 16;
            result |= DescHash(desc._format) << 24;
            result ^= DescHash(desc._mipCount) << 50;   // (gets interleaved with the pixel format -- could be trouble?)
        } else {
            uint32 temp = Hash32(&desc, PtrAdd(&desc, sizeof(desc)));
            result = DescHash(temp)<<8;
            result &= ~0x1; // clear the bottom "type" bit
        }
        return result;
    }

    static unsigned RoundUpBufferSize(unsigned input)
    {
        unsigned log2 = IntegerLog2(input);
        if ((1<<log2)==input) {
            return input;
        }
        unsigned nextBit = 1<<(log2-1);
        unsigned nextBit2 = 1<<(log2-2);
        if (log2 >= 14 && !(input & nextBit2)) {
            return (input&((1<<log2)|nextBit))|nextBit2;
        }
        if (log2 >= 12 && !(input & nextBit)) {
            return (1<<log2)|nextBit;
        }
        return 1<<(log2+1);
    }

    static DescHash Hash(const BufferDesc& desc)
    {
        DescHash result = 0;
        result |= desc._type&0x1;
        result |= (desc._cpuAccess&0x7)<<1;
        result |= (desc._gpuAccess&0x3)<<4;
        result |= (desc._bindFlags&0x3)<<6;     // "Shader Resource" gets ignored here. We need more room for more advanced bind flags
        if (desc._type == BufferDesc::Type::Texture) {
            result |= Hash(desc._textureDesc) << 8;
        } else if (desc._type == BufferDesc::Type::LinearBuffer) {
            result |= desc._linearBufferDesc._sizeInBytes << 8;
        }
        return result;
    }

    // ~~~~~~~~~~~~ // ~~~~~~<   >~~~~~~ // ~~~~~~~~~~~~ //

        /////   R E S O U R C E S   P O O L   /////

    #define tdesc template<typename Desc>

    tdesc auto ResourcesPool<Desc>::PoolOfLikeResources::AllocateResource(unsigned realSize, bool& deviceCreation) -> UnderlyingResourcePtr
        {
            Entry* front = NULL;
            if (_allocableResources.try_front(front)) {
                Interlocked::Increment(&_recentPoolCreateCount);
                deviceCreation = false;
                UnderlyingResourcePtr result(std::move(front->_underlying));
                _allocableResources.pop();
                return result;
            } else {
                if (deviceCreation) {
                    Interlocked::Increment(&_recentDeviceCreateCount);
                    deviceCreation = true;
                    auto result = PlatformInterface::CreateResource(*_underlyingDevice, _desc, NULL);
                    if (result) {
                        #if defined(REUSABLE_RESOURCE_DEBUGGING)
                            PlatformInterface::AttachObject(
                                result, GUID_ReusableResourceDestructionHelper, 
                                new ReusableResourceDestructionHelper(_desc));
                        #endif
                    } else {
                        LogAlwaysWarningF("Warning -- failed when creating a resource in PoolOfLikeResources::AllocateResource");
                    }
                    Interlocked::Add(&_totalRealSize, realSize);
                    Interlocked::Add(&_totalCreateSize, RenderCore::ByteCount(_desc));
                    Interlocked::Increment(&_totalCreateCount);
                    return std::move(result);
                }
            }
            return nullptr;
        }

    tdesc void ResourcesPool<Desc>::PoolOfLikeResources::Update(unsigned newFrameID)
        {
            _currentFrameID = newFrameID;
                // pop off any resources that have lived here for too long
            if (_retainFrames != ~unsigned(0x0)) {
                const unsigned minToKeep = 4;
                while (_allocableResources.size() > minToKeep) {
                    Entry* front = NULL;
                    if (!_allocableResources.try_front(front) || (newFrameID - front->_returnFrameID) < _retainFrames) {
                        break;
                    }

                    #if defined(REUSABLE_RESOURCE_DEBUGGING)
                        Interlocked::Value oldOkToDeleteReusableResources = Interlocked::Exchange(&g_oldToDeleteReusableResources, 1);
                    #endif

                    _allocableResources.pop();

                    #if defined(REUSABLE_RESOURCE_DEBUGGING)
                        Interlocked::Exchange(&g_oldToDeleteReusableResources, oldOkToDeleteReusableResources);
                    #endif
                }
            }
        }

    tdesc void ResourcesPool<Desc>::PoolOfLikeResources::ReturnToPool(UnderlyingResourcePtr&& resource)
        {
            Entry newEntry;
            newEntry._underlying = std::forward<UnderlyingResourcePtr>(resource);
            newEntry._returnFrameID = _currentFrameID;
            _allocableResources.push(newEntry);
            Interlocked::Increment(&_recentReleaseCount);
        }

    tdesc ResourcesPool<Desc>::PoolOfLikeResources::PoolOfLikeResources(
			RenderCore::IDevice& underlyingDevice, const Desc& desc, unsigned retainFrames) : _desc(desc)
        {
            _peakSize = 0;
            _recentDeviceCreateCount = _recentPoolCreateCount = _recentReleaseCount = 0;
            _totalCreateSize = _totalCreateCount = _totalRealSize = 0;
            _currentFrameID = 0;
            _retainFrames = retainFrames;
            _underlyingDevice = &underlyingDevice;
        }

    tdesc ResourcesPool<Desc>::PoolOfLikeResources::~PoolOfLikeResources()
        {
        }

    tdesc PoolMetrics    ResourcesPool<Desc>::PoolOfLikeResources::CalculateMetrics() const
    {
        PoolMetrics result;
        result._desc = _desc;
        {
            //ScopedLock(_lock);
            result._currentSize = (unsigned)_allocableResources.size();
        }
        result._peakSize = _peakSize = std::max(_peakSize, result._currentSize);
        result._topMostAge               = 0;
        result._recentDeviceCreateCount  = Interlocked::Exchange(&_recentDeviceCreateCount, 0);
        result._recentPoolCreateCount    = Interlocked::Exchange(&_recentPoolCreateCount, 0);
        result._recentReleaseCount       = Interlocked::Exchange(&_recentReleaseCount, 0);
        result._totalRealSize            = _totalRealSize;
        result._totalCreateSize          = _totalCreateSize;
        result._totalCreateCount         = _totalCreateCount;
        return result;
    }

    tdesc ResourcesPool<Desc>::ResourcesPool(RenderCore::IDevice* device, unsigned retainFrames) 
	: _hashTableIndex(0), _retainFrames(retainFrames), _underlyingDevice(device)
    {
        _readerCount[0] = _readerCount[1] = 0;
    }
    
    tdesc ResourcesPool<Desc>::~ResourcesPool() {}

    tdesc intrusive_ptr<ResourceLocator>   ResourcesPool<Desc>::CreateResource(
            const Desc& desc, unsigned realSize, bool& deviceCreation)
        {
            DescHash hashValue = Hash(desc);
            {
                unsigned hashTableIndex = _hashTableIndex;
                Interlocked::Increment(&_readerCount[hashTableIndex]);
                HashTable& hashTable = _hashTables[hashTableIndex];
                auto entry = std::lower_bound(hashTable.begin(), hashTable.end(), hashValue, CompareFirst());
                if (entry != hashTable.end() && entry->first == hashValue) {
                    if (desc._type == BufferDesc::Type::Texture) {
                        assert(desc._textureDesc._width == entry->second->GetDesc()._textureDesc._width);
                        assert(desc._textureDesc._height == entry->second->GetDesc()._textureDesc._height);
                        assert(desc._textureDesc._mipCount == entry->second->GetDesc()._textureDesc._mipCount);
                        assert(desc._textureDesc._format == entry->second->GetDesc()._textureDesc._format);
                    }
                    auto newResource = entry->second.get()->AllocateResource(realSize, deviceCreation);
                    Interlocked::Decrement(&_readerCount[hashTableIndex]);
                    return make_intrusive<ResourceLocator>(
                        std::move(newResource), ~unsigned(0x0), 0,
                        shared_from_this(), hashValue);
                }
                Interlocked::Decrement(&_readerCount[hashTableIndex]);
            }

            if (!deviceCreation) {
                return nullptr;
            }

                //
                //              -=*=- Insert a new hash table entry for this type of resource -=*=-
                //
            {
                ScopedLock(_writerLock);

                    //
                    //      Doubled buffered writing scheme... We don't change the hash table very often, so let's optimise
                    //      for when the hash table isn't modified. 
                    //      Note there might be a problem here if we get a reader that uses a hash table while another thread
                    //      has enough table to modify the hash tables twice.
                    //
                unsigned oldHashTableIndex = _hashTableIndex;
                unsigned nextHashTableIndex = (oldHashTableIndex+1)%dimof(_hashTables);
                HashTable& newHashTable = _hashTables[nextHashTableIndex];
                newHashTable = _hashTables[oldHashTableIndex];
                auto entry = std::lower_bound(newHashTable.begin(), newHashTable.end(), hashValue, CompareFirst());

                HashTableEntry newEntry;
                newEntry.first = hashValue;
                newEntry.second = std::make_shared<PoolOfLikeResources>(
                    std::ref(*_underlyingDevice), desc, _retainFrames);
                auto newIterator = newHashTable.insert(entry, newEntry);
                _hashTableIndex = nextHashTableIndex;

                auto newResource = newIterator->second->AllocateResource(realSize, deviceCreation);

                    //  We should wait until there are no more readers on the old hash table before we give up the "_writerLock" mutex. This is because
                    //  Another thread could create a new entry immediately after, and start writing over the old hash table while readers are still
                    //  using it (even though this is very rare, it still does happen). But if we wait until that hash table is no longer being used, we're safe 
                    //  to release the _writerLock. Unfortunately it requires an extra interlocked Increment/Decrement in every read operation...
                while (_readerCount[oldHashTableIndex]) {}

                return make_intrusive<ResourceLocator>(
                    std::move(newResource), 
                    0, ~unsigned(0x0), shared_from_this(), hashValue);
            }
        }

    tdesc void        ResourcesPool<Desc>::AddRef(
            uint64 resourceMarker, UnderlyingResource* resource, 
            unsigned offset, unsigned size)
        {
            // we don't have to do anything in this case
        }
    
    tdesc void        ResourcesPool<Desc>::ReturnToPool(
            uint64 resourceMarker, UnderlyingResourcePtr&& resource, 
            unsigned offset, unsigned size)
        {
            unsigned hashTableIndex = _hashTableIndex;
            Interlocked::Increment(&_readerCount[hashTableIndex]);
            HashTable& hashTable = _hashTables[hashTableIndex];
            auto entry = std::lower_bound(hashTable.begin(), hashTable.end(), resourceMarker, CompareFirst());
            if (entry != hashTable.end() && entry->first == resourceMarker) {
                entry->second->ReturnToPool(std::forward<UnderlyingResourcePtr>(resource));
                Interlocked::Decrement(&_readerCount[hashTableIndex]);
            } else {
                Interlocked::Decrement(&_readerCount[hashTableIndex]);
            }
        }

    tdesc void        ResourcesPool<Desc>::Update(unsigned newFrameID)
        {
            unsigned hashTableIndex = _hashTableIndex;
            Interlocked::Increment(&_readerCount[hashTableIndex]);
            HashTable& hashTable = _hashTables[hashTableIndex];
            for (auto i=hashTable.begin(); i!=hashTable.end(); ++i) {
                i->second->Update(newFrameID);
            }
            Interlocked::Decrement(&_readerCount[hashTableIndex]);
        }

    tdesc std::vector<PoolMetrics>        ResourcesPool<Desc>::CalculateMetrics() const
    {
        ScopedLock(_writerLock);
        const HashTable& hashTable = _hashTables[_hashTableIndex];
        std::vector<PoolMetrics> result;
        result.reserve(hashTable.size());
        for (auto i = hashTable.begin(); i!= hashTable.end(); ++i) {
            result.push_back(i->second->CalculateMetrics());
        }
        return result;
    }

    tdesc void            ResourcesPool<Desc>::OnLostDevice()
    {
        ScopedLock(_writerLock);
        for (unsigned c=0; c<dimof(_hashTables); ++c) {
            while (_readerCount[c]) {}
            _hashTables[c] = HashTable();
        }
        _hashTableIndex = 0;
    }

        /////   B A T C H E D   R E S O U R C E S   /////

    intrusive_ptr<ResourceLocator>    BatchedResources::Allocate(
        unsigned size, bool& deviceCreation, const char name[])
    {
        if (size > RenderCore::ByteCount(_prototype)) {
            deviceCreation = false;
            return nullptr;
        }

        {
            ScopedReadLock(_lock);
            {
                ScopedLock(_activeDefrag_Lock);  // prevent _activeDefragHeap from changing while doing this...
                                                //  (we can't allocate from a heap that is currently being defragged)
                // for (std::vector<HeapedResource*>::reverse_iterator i=_heaps.rbegin(); i!=_heaps.rend(); ++i) {
                //     if ((*i) != _activeDefragHeap) {
                //         assert(!_activeDefrag.get() || _activeDefrag->GetHeap()!=*i);
                //         unsigned allocation = (*i)->Allocate(size);
                //         if (allocation != ~unsigned(0x0)) {
                //             deviceCreation = false;
                //             assert((allocation+size)<=PlatformInterface::ByteCount(_prototype));
                //             return ResourceLocator((*i)->_heapResource, allocation, size);
                //         }
                //     }
                // }

                HeapedResource* bestHeap = NULL;
                unsigned bestHeapLargestBlock = ~unsigned(0x0);
                for (auto i=_heaps.rbegin(); i!=_heaps.rend(); ++i) {
                    if (i->get() != _activeDefragHeap) {
                        assert(!_activeDefrag.get() || _activeDefrag->GetHeap()!=i->get());
                        unsigned largestBlock = (*i)->_heap.CalculateLargestFreeBlock();
                        if (largestBlock >= size && largestBlock < bestHeapLargestBlock) {
                            bestHeap = i->get();
                            bestHeapLargestBlock = largestBlock;
                        }
                    }
                }

                if (bestHeap) {
                    unsigned allocation = bestHeap->Allocate(size, name);
                    if (allocation != ~unsigned(0x0)) {
                        deviceCreation = false;
                        assert((allocation+size)<=RenderCore::ByteCount(_prototype));
                        return make_intrusive<ResourceLocator>(
                            bestHeap->_heapResource->ShareUnderlying(), 
                            allocation, size, 
                            shared_from_this());
                    }
                }
            }
        }

        auto heapResource = _sourcePool->CreateResource(
            _prototype, _prototype._linearBufferDesc._sizeInBytes, deviceCreation);
        if (!heapResource || heapResource->IsEmpty()) {
            deviceCreation = false;
            return nullptr;
        }

        auto newHeap = std::make_unique<HeapedResource>(_prototype, heapResource);
        unsigned allocation = newHeap->Allocate(size, name);
        assert(allocation != ~unsigned(0x0));

        auto result = make_intrusive<ResourceLocator>(
            newHeap->_heapResource->ShareUnderlying(), allocation, size, shared_from_this());
        ScopedModifyLock(_lock);
        _heaps.push_back(std::move(newHeap));
        return result;
    }
    
    void BatchedResources::AddRef(
        uint64 resourceMarker, UnderlyingResource* resource, 
        unsigned offset, unsigned size)
    {
        if (!resource) return;

        ScopedReadLock(_lock);
        HeapedResource* heap = NULL;
        if (_activeDefrag.get() && _activeDefrag->GetHeap()->_heapResource->GetUnderlying() == resource) {
            heap = _activeDefrag->GetHeap();
        } else {
            for (auto i=_heaps.rbegin(); i!=_heaps.rend(); ++i) {
                if ((*i)->_heapResource->GetUnderlying() == resource) {
                    heap = i->get();
                    break;
                }
            }
        }

        if (heap) {
            heap->AddRef(offset, size, "<<unknown>>");
        } else {
            assert(0);
        }
    }

    void BatchedResources::ReturnToPool(
        uint64 resourceMarker, UnderlyingResourcePtr&& resource, 
        unsigned offset, unsigned size)
    {
        if (!resource) return;

        ScopedReadLock(_lock);
        HeapedResource* heap = NULL;
        if (_activeDefrag.get() && _activeDefrag->GetHeap()->_heapResource->GetUnderlying() == resource.get()) {
            heap = _activeDefrag->GetHeap();
        } else {
            for (auto i=_heaps.rbegin(); i!=_heaps.rend(); ++i) {
                if ((*i)->_heapResource->GetUnderlying() == resource.get()) {
                    heap = i->get();
                    break;
                }
            }
        }

        if (heap) {
            if (heap->Deref(offset, size)) {
                assert(!_activeDefrag.get() || heap != _activeDefrag->GetHeap());
                if (_activeDefragHeap == heap) {
                    _activeDefrag->QueueOperation(
                        ActiveDefrag::Operation::Deallocate, offset, offset+size);
                } else {
                    heap->Deallocate(offset, size);
                }
                #if defined(XL_DEBUG)
                    heap->ValidateRefsAndHeap();
                #endif
            }

                // (prevent caller from performing extra derefs)
            resource = nullptr;
        }
    }

    BatchedResources::ResultFlags::BitField BatchedResources::IsBatchedResource(
        UnderlyingResource* resource) const
    {
        ScopedReadLock(_lock);
        if (_activeDefrag.get() && _activeDefrag->GetHeap()->_heapResource->GetUnderlying() == resource) {
            return ResultFlags::IsBatched;
        }
        for (auto i=_heaps.rbegin(); i!=_heaps.rend(); ++i) {
            if ((*i)->_heapResource->GetUnderlying() == resource) {
                return ResultFlags::IsBatched|(i->get()==_activeDefragHeap?ResultFlags::IsCurrentlyDefragging:0);
            }
        }
        return 0;
    }

    BatchedResources::ResultFlags::BitField BatchedResources::Validate(const ResourceLocator& locator) const
    {
        ScopedReadLock(_lock);

            //      check to make sure the same resource isn't showing up twice
        for (auto i=_heaps.begin(); i!=_heaps.end(); ++i) {
            for (auto i2=i+1; i2!=_heaps.end(); ++i2) {
                assert((*i2)->_heapResource != (*i)->_heapResource);
            }
        }

        BatchedResources::ResultFlags::BitField result = 0;
        const HeapedResource* heapResource = NULL;
        if (_activeDefrag.get() && _activeDefrag->GetHeap()->_heapResource->GetUnderlying() == locator.GetUnderlying()) {
            heapResource = _activeDefrag->GetHeap();
        } else {
            for (auto i=_heaps.rbegin(); i!=_heaps.rend(); ++i) {
                if ((*i)->_heapResource->GetUnderlying() == locator.GetUnderlying()) {
                    heapResource = i->get();
                    break;
                }
            }
        }
        if (heapResource) {
            result |= BatchedResources::ResultFlags::IsBatched;
            assert(heapResource->_refCounts.ValidateBlock(locator.Offset(), locator.Size()));
        }
        return result;
    }

    BatchingSystemMetrics BatchedResources::CalculateMetrics() const
    {
        ScopedReadLock(_lock);
        BatchingSystemMetrics result;
        result._heaps.reserve(_heaps.size());
        for (auto i=_heaps.begin(); i!=_heaps.end(); ++i) {
            result._heaps.push_back((*i)->CalculateMetrics());
        }
        return result;
    }

    void BatchedResources::TickDefrag(ThreadContext& context, IManager::EventListID processedEventList, bool& deviceCreation)
    {
        bool isDeviceCreationOk = deviceCreation;
        deviceCreation = false;

        if (!_activeDefrag.get()) {

            assert(!_activeDefragHeap);

                    //                            -                                 //
                    //                         -------                              //
                    //                                                              //
                //////////////////////////////////////////////////////////////////////////
                    //      Start a new defrag step on the most fragmented heap     //
                    //                            -                                 //
                    //        Note that we defrag the allocated spans, rather       //
                    //        than the blocks. This means that adjacent blocks      //
                    //        always move with each other, regardless of            //
                    //        their size, and ideal finish position.                //
                    //                            -                                 //
                    //        On slower PCs we can end up consuming a lot of        //
                    //        time just doing the defrags. So we need to            //
                    //        throttle it a bit, and so that we only do the         //
                    //        defrag when we really need it.                        //
                //////////////////////////////////////////////////////////////////////////
                    //                                                              //
                    //                         -------                              //
                    //                            -                                 //

            const float minWeightToDoSomething = 20.f * 1024.f;   // only do something when there's a 20k difference between total available space and the largest block
            float bestWeight = minWeightToDoSomething;
            HeapedResource* bestHeap = NULL;
            {
                ScopedReadLock(_lock);
                for (auto i=_heaps.begin(); i!=_heaps.end(); ++i) {
                    float weight = (*i)->CalculateFragmentationWeight();
                    if (weight > bestWeight && (*i)->_heapResource.get()) {
                            //      if the heap hasn't changed since the last time this heap was used as a defrag source, then there's no use in picking it again
                        if ((*i)->_hashLastDefrag != (*i)->_heap.CalculateHash()) {
                            bestHeap = i->get();
                            bestWeight = weight;
                            break;
                        }
                    }
                }
            }

                                //      -=-=-=-=-=-=-=-=-=-=-       //

            if (bestHeap) {
                {
                    ScopedLock(_activeDefrag_Lock);  // must lock during the defrag commit & defrag create
                    _activeDefrag.reset(new ActiveDefrag());
                    _activeDefragHeap = bestHeap;
                }

                    // Now that we've set bestHeap->_activeDefrag, bestHeap->_heap is immutable...
                _activeDefrag->SetSteps(bestHeap->_heap, bestHeap->_heap.CalculateDefragSteps());
                bestHeap->_hashLastDefrag = bestHeap->_heap.CalculateHash();

                    // Copy the resource into our copy buffer, and set the count down
                if (PlatformInterface::UseMapBasedDefrag && !PlatformInterface::CanDoNooverwriteMapInBackground) {
                    context.GetDeviceContext().ResourceCopy(
                        *_temporaryCopyBuffer->GetUnderlying(), *bestHeap->_heapResource->GetUnderlying());
                    _temporaryCopyBufferCountDown = 10;
                }

                #if defined(XL_DEBUG)
                    unsigned blockCount = bestHeap->_refCounts.GetEntryCount();
                    for (unsigned b=0; b<blockCount; ++b) {
                        std::pair<unsigned,unsigned> block = bestHeap->_refCounts.GetEntry(b);
                        bool foundOne = false;
                        for (std::vector<DefragStep>::const_iterator i =_activeDefrag->GetSteps().begin(); i!=_activeDefrag->GetSteps().end(); ++i) {
                            if (block.first >= i->_sourceStart && block.second <= i->_sourceEnd) {
                                foundOne = true;
                                break;
                            }
                        }
                        assert(foundOne);
                    }
                #endif
            }

        } else {

                //
                //      Check on the status of the defrag step; and commit to the 
                //      active resource as necessary
                //
            ActiveDefrag* existingActiveDefrag = _activeDefrag.get();
            if (!existingActiveDefrag->GetHeap()->_heapResource) {

                    //////      Try to find a heap that is 100% free. We'll remove        //
                      //          this from the list, and use it as our new heap        //////

                {
                    ScopedModifyLock(_lock);
                    for (auto i=_heaps.begin(); i!=_heaps.end(); ++i) {
                        if (i->get() != _activeDefragHeap && (*i)->_heap.IsEmpty()) {
                            existingActiveDefrag->GetHeap()->_heapResource.swap((*i)->_heapResource);
                            _heaps.erase(i);
                            break;
                        }
                    }
                }

                if (!existingActiveDefrag->GetHeap()->_heapResource) {
                    existingActiveDefrag->GetHeap()->_heapResource = _sourcePool->CreateResource(
                        _prototype, RenderCore::ByteCount(_prototype), isDeviceCreationOk);
                    deviceCreation = isDeviceCreationOk;
                }

            } else {
                
                if (_temporaryCopyBufferCountDown > 0) {
                    --_temporaryCopyBufferCountDown;
                } else {
                    const bool useTemporaryCopyBuffer = PlatformInterface::UseMapBasedDefrag && !PlatformInterface::CanDoNooverwriteMapInBackground;
                    existingActiveDefrag->Tick(context, useTemporaryCopyBuffer?_temporaryCopyBuffer->ShareUnderlying():_activeDefragHeap->_heapResource->ShareUnderlying());
                    if (existingActiveDefrag->IsCompleted(processedEventList, context)) {

                            //
                            //      Everything should be de-reffed from the original heap.
                            //          Sometimes there appears to be leaks here... We could just leave the old
                            //          heap as part of our list of heaps. It would then just be filled up
                            //          again with future allocations.
                            //
                        // assert(_activeDefragHeap->_refCounts.CalculatedReferencedSpace()==0);        it's ok now

                            //
                            //      Signal client which blocks that have moved; and change the 
                            //      _heapResource value of the real heap
                            //
                        _activeDefrag->ReleaseSteps();
                        _activeDefrag->ApplyPendingOperations(*_activeDefragHeap);
                        _activeDefrag->GetHeap()->_defragCount = _activeDefragHeap->_defragCount+1;
                        // assert(_activeDefragHeap->_heap.IsEmpty());      // it's ok now

                        ScopedModifyLock(_lock); // lock here to prevent any operations on _activeDefrag->GetHeap() while we do this...
                        _heaps.push_back(_activeDefrag->ReleaseHeap());

                        {
                            ScopedLock(_activeDefrag_Lock);
                            _activeDefrag.reset(NULL);
                            _activeDefragHeap = NULL;
                        }
                    }
                }
            }
        }
    }

    void               BatchedResources::OnLostDevice()
    {
        ScopedModifyLock(_lock);
        {
            ScopedLock(_activeDefrag_Lock);
            _activeDefrag.reset();
            _activeDefragHeap = NULL;
        }

        _temporaryCopyBuffer.reset();
        _temporaryCopyBufferCountDown = 0;

        _heaps.clear();
        _sourcePool->OnLostDevice();
    }

    BatchedResources::BatchedResources(const BufferDesc& prototype, std::shared_ptr<ResourcesPool<BufferDesc>> sourcePool)
    :       _prototype(prototype)
    ,       _sourcePool(std::move(sourcePool))
    ,       _activeDefrag(nullptr)
    ,       _activeDefragHeap(NULL)
    {
        BufferDesc copyBufferDesc = prototype;
        copyBufferDesc._cpuAccess = CPUAccess::Read;
        copyBufferDesc._gpuAccess = 0;
        copyBufferDesc._allocationRules = AllocationRules::Staging;
        copyBufferDesc._bindFlags = 0;

        _temporaryCopyBuffer = NULL;
        _temporaryCopyBufferCountDown = 0;
        if (PlatformInterface::UseMapBasedDefrag && !PlatformInterface::CanDoNooverwriteMapInBackground) {
            _temporaryCopyBuffer = make_intrusive<ResourceLocator>(
                PlatformInterface::CreateResource(*sourcePool->GetUnderlyingDevice(), copyBufferDesc));
        }
    }

    BatchedResources::~BatchedResources()
    {}

    unsigned    BatchedResources::HeapedResource::Allocate(unsigned size, const char name[])
    {
        unsigned allocation =_heap.Allocate(size);
        if (allocation != ~unsigned(0x0)) {
            _heapResource->AddRef();
            _refCounts.AddRef(allocation, size, name);
        }
        return allocation;
    }

    bool        BatchedResources::HeapedResource::AddRef(unsigned ptr, unsigned size, const char name[])
    {
        std::pair<signed,signed> newRefCounts = _refCounts.AddRef(ptr, size, name);
        _heapResource->AddRef();
        assert(newRefCounts.first >= 0 && newRefCounts.second >= 0);
        assert(newRefCounts.first == newRefCounts.second);
        return newRefCounts.second==1;
    }
    
    bool        BatchedResources::HeapedResource::Deref(unsigned ptr, unsigned size)
    {
        std::pair<signed,signed> newRefCounts = _refCounts.Release(ptr, size);
        unsigned long newResourceRefCount = _heapResource->Release();    // Always pull off a reference from the heap resource
        assert(newResourceRefCount!=0); (void)newResourceRefCount;                        // we should never destroy the object with this release
        assert(newRefCounts.first >= 0 && newRefCounts.second >= 0);
        assert(newRefCounts.first == newRefCounts.second);
        return newRefCounts.second==0;
    }

    void    BatchedResources::HeapedResource::Allocate(unsigned ptr, unsigned size)
    {
        _heap.Allocate(ptr, size);
    }

    void        BatchedResources::HeapedResource::Deallocate(unsigned ptr, unsigned size)
    {
        _heap.Deallocate(ptr, size);
    }

    BatchedHeapMetrics BatchedResources::HeapedResource::CalculateMetrics() const
    {
        BatchedHeapMetrics result;
        result._markers          = _heap.CalculateMetrics();
        result._allocatedSpace   = result._unallocatedSpace = 0;
        result._heapSize         = _size;
        result._largestFreeBlock = result._spaceInReferencedCountedBlocks = result._referencedCountedBlockCount = 0;

        unsigned previousStart = 0;
        for (std::vector<unsigned>::iterator i=result._markers.begin(); (i+1)<result._markers.end(); i+=2) {
            unsigned start = *i, end = *(i+1);
            result._allocatedSpace   += start-previousStart;
            result._unallocatedSpace += end-start;
            result._largestFreeBlock  = std::max(result._largestFreeBlock, end-start);
            previousStart = end;
        }

        result._spaceInReferencedCountedBlocks   = _refCounts.CalculatedReferencedSpace();
        result._referencedCountedBlockCount      = _refCounts.GetEntryCount();
        return result;
    }

    float BatchedResources::HeapedResource::CalculateFragmentationWeight() const
    {
        unsigned largestBlock    = _heap.CalculateLargestFreeBlock();
        unsigned availableSpace  = _heap.CalculateAvailableSpace();
        if (largestBlock > .5f * availableSpace) {
            return 0.f;
        }
        return float(availableSpace - largestBlock);
    }

    void BatchedResources::HeapedResource::ValidateRefsAndHeap()
    {
            //
            //      Check to make sure that the reference counting layer and the heap agree.
            //      There might be some discrepancies during defragging because of the delayed
            //      Deallocate. But otherwise they should match up.
            //
        #if defined(_DEBUG)
            unsigned referencedSpace = _refCounts.CalculatedReferencedSpace();
            unsigned heapAllocatedSpace = _heap.CalculateAllocatedSpace();
            assert(heapAllocatedSpace == referencedSpace);
        #endif
    }

    BatchedResources::HeapedResource::HeapedResource()
    : _size(0), _defragCount(0), _heap(0), _refCounts(0), _hashLastDefrag(0)
    {}

    BatchedResources::HeapedResource::HeapedResource(const BufferDesc& desc, const intrusive_ptr<ResourceLocator>& heapResource)
    : _heapResource(heapResource)
    , _heap(RenderCore::ByteCount(desc))
    , _refCounts(RenderCore::ByteCount(desc))
    , _size(RenderCore::ByteCount(desc))
    , _defragCount(0)
    , _hashLastDefrag(0)
    {}

    BatchedResources::HeapedResource::~HeapedResource()
    {
        #if defined(XL_DEBUG)
            ValidateRefsAndHeap();
            if (_refCounts.GetEntryCount()) {
                assert(0);  // something leaked!
            }
        #endif
        _heapResource.reset();
    }

    void BatchedResources::ActiveDefrag::QueueOperation(Operation::Enum operation, unsigned start, unsigned end)
    {
        assert(end>start);
        PendingOperation op;
        op._operation = operation;
        op._start = start;
        op._end = end;
        _pendingOperations.push_back(op);
    }

    void BatchedResources::ActiveDefrag::Tick(ThreadContext& context, const UnderlyingResourcePtr& sourceResource)
    {
        if (!_initialCommandListID) {
            _initialCommandListID = context.CommandList_GetUnderConstruction();
        }
        if (!_steps.empty() && GetHeap()->_heapResource && !_doneResourceCopy) {
                // -----<   Copy from the old resource into the new resource   >----- //
            if (PlatformInterface::UseMapBasedDefrag && !PlatformInterface::CanDoNooverwriteMapInBackground) {
                context.GetCommitStepUnderConstruction().Add(
                    CommitStep::DeferredDefragCopy(GetHeap()->_heapResource->ShareUnderlying(), sourceResource, _steps));
            } else {
                context.GetDeviceContext().ResourceCopy_DefragSteps(*GetHeap()->_heapResource->GetUnderlying(), *sourceResource, _steps);
            }
            _doneResourceCopy = true;
        }

        if (_doneResourceCopy && !_eventId && context.CommandList_GetCommittedToImmediate() >= _initialCommandListID) {
            Event_ResourceReposition result;
            result._originalResource = sourceResource;
            result._newResource      = GetHeap()->_heapResource->ShareUnderlying();
            result._defragSteps      = _steps;
            _eventId = context.EventList_Push(result);
        }
    }

    void BatchedResources::ActiveDefrag::SetSteps(const SimpleSpanningHeap& sourceHeap, const std::vector<DefragStep>& steps)
    {
        assert(_steps.empty());      // can't change the steps once they're specified!
        _steps = steps;
        _newHeap->_size = sourceHeap.CalculateHeapSize();
        _newHeap->_heap = SimpleSpanningHeap(_newHeap->_size);

        #if defined(_DEBUG)
            for (std::vector<DefragStep>::const_iterator i=_steps.begin(); i!=_steps.end(); ++i) {
                unsigned end = i->_destination + i->_sourceEnd - i->_sourceStart;
                assert(end<=_newHeap->_size);
            }
        #endif
    }

    void BatchedResources::ActiveDefrag::ReleaseSteps()
    {
        _steps.clear();
    }

    void BatchedResources::ActiveDefrag::ApplyPendingOperations(HeapedResource& destination)
    {
        if (_pendingOperations.empty()) {
            return;
        }

        for (std::vector<ActiveDefrag::PendingOperation>::iterator deallocateIterator = _pendingOperations.begin(); deallocateIterator != _pendingOperations.end(); ++deallocateIterator) {
            destination.Deallocate(deallocateIterator->_start, deallocateIterator->_end-deallocateIterator->_start);
        }

        #if 0
            std::sort(_pendingOperations.begin(), _pendingOperations.end(), SortByPosition);
            std::vector<ActiveDefrag::PendingOperation>::iterator deallocateIterator = _pendingOperations.begin();
            for (std::vector<DefragStep>::const_iterator s=_steps.begin(); s!=_steps.end() && deallocateIterator!=_pendingOperations.end();) {
                if (s->_sourceEnd <= deallocateIterator->_start) {
                    ++s;
                    continue;
                }

                if (s->_sourceStart >= (deallocateIterator->_end)) {
                        //      This deallocate iterator doesn't have an adjustment
                    ++deallocateIterator;
                    continue;
                }

                    //
                    //      We shouldn't have any blocks that are stretched between multiple 
                    //      steps. If we've got a match it must match the entire deallocation block
                    //
                assert(deallocateIterator->_start >= s->_sourceStart && deallocateIterator->_start < s->_sourceEnd);
                assert((deallocateIterator->_end) > s->_sourceStart && (deallocateIterator->_end) <= s->_sourceEnd);

                signed offset = s->_destination - signed(s->_sourceStart);
                deallocateIterator->_start += offset;
                ++deallocateIterator;
            }

                //
                //      Now just deallocate those blocks... But note we've just done a defrag pass, so this
                //      will just create new gaps!
                //
            for (deallocateIterator = _pendingOperations.begin(); deallocateIterator != _pendingOperations.end(); ++deallocateIterator) {
                GetHeap()->Deallocate(deallocateIterator->_start, deallocateIterator->_end-deallocateIterator->_start);
            }
        #endif
    }

    bool BatchedResources::ActiveDefrag::IsCompleted(IManager::EventListID processedEventList, ThreadContext& context)
    {
        return  GetHeap()->_heapResource && _doneResourceCopy && (processedEventList >= _eventId) 
            &&  (context.CommandList_GetCompletedByGPU() >= _initialCommandListID);
    }

    auto BatchedResources::ActiveDefrag::ReleaseHeap() -> std::unique_ptr<HeapedResource>&&
    {
        return std::move(_newHeap);
    }

    BatchedResources::ActiveDefrag::ActiveDefrag()
    : _doneResourceCopy(false), _eventId(0)
    , _newHeap(new HeapedResource())
    , _initialCommandListID(0)
    {
    }

    BatchedResources::ActiveDefrag::~ActiveDefrag()
    {
        ReleaseSteps();
    }

    bool BatchedResources::ActiveDefrag::SortByPosition(const PendingOperation& lhs, const PendingOperation& rhs) { return lhs._start < rhs._start; }

    // static string Description(const BufferDesc& desc)
    // {
    //     char buffer[2048];
    //     if (desc._type == BufferDesc::Type::Texture) {
    //         _snprintf_s(buffer, _TRUNCATE, "Tex (%ix%i) [%s]", desc._textureDesc._width, desc._textureDesc._height, desc._name);
    //     } else if (desc._type == BufferDesc::Type::LinearBuffer) {
    //         _snprintf_s(buffer, _TRUNCATE, "Buffer (%.3fKB)", desc._linearBufferDesc._sizeInBytes/1024.f);
    //     }
    //     return string(buffer);
    // }

        /////   R E S O U R C E   S O U R C E   /////

    ResourceSource::ResourceConstruction        ResourceSource::Create(const BufferDesc& desc, DataPacket* initialisationData, CreationOptions::BitField options)
    {
        bool deviceCreation          = !!(options & CreationOptions::AllowDeviceCreation);
        // const UploadDataType::Enum uploadDataType = AsUploadDataType(desc);
        const bool usePooling        = UsePooling(desc);
        const bool useBatching       = usePooling && UseBatching(desc) && initialisationData;
        const bool useStaging        = !!(desc._allocationRules & AllocationRules::Staging);
        const unsigned objectSize    = RenderCore::ByteCount(desc);

        ResourceConstruction result;
        if (useStaging) {
            result._identifier = _stagingBufferPool->CreateResource(AdjustDescForReusableResource(desc), objectSize, deviceCreation);
        } else if (!useBatching) {
            if (usePooling) {
                result._identifier = _pooledGeometryBuffers->CreateResource(AdjustDescForReusableResource(desc), objectSize, deviceCreation);
                result._flags |= deviceCreation?ResourceConstruction::Flags::DeviceConstructionInvoked:0;
            } else if (deviceCreation) {
				auto supportInit = 
					(desc._type == BufferDesc::Type::Texture)
					? PlatformInterface::SupportsResourceInitialisation_Texture
					: PlatformInterface::SupportsResourceInitialisation_Buffer;
				auto initPkt = supportInit ? initialisationData : nullptr;

                result._identifier = make_intrusive<ResourceLocator>(
                    PlatformInterface::CreateResource(*_underlyingDevice, desc, initPkt),
                    0, objectSize);
                result._flags |= initPkt ? ResourceConstruction::Flags::InitialisationSuccessful : 0;
                result._flags |= ResourceConstruction::Flags::DeviceConstructionInvoked;
                if (!result._identifier || result._identifier->IsEmpty()) {
                    // LogAlwaysWarningF("Warning -- Failed to create device buffer for resource (%s)", Description(desc).c_str());
                }
            }
        } else {
            result._flags |= ResourceConstruction::Flags::DelayForBatching;
        }
        return result;
    }

    bool                ResourceSource::WillBeBatched(const BufferDesc& desc)
    {
        return UsePooling(desc) && UseBatching(desc);
    }

    BatchedResources::ResultFlags::BitField ResourceSource::IsBatchedResource(const ResourceLocator& locator, const BufferDesc& desc)
    {
        const bool mightBeBatched = UsePooling(desc) && UseBatching(desc);
        if (mightBeBatched) {
            return _batchedIndexBuffers->IsBatchedResource(locator.GetUnderlying());
        }
        return 0;
    }

    void ResourceSource::Validate(const ResourceLocator& locator)
    {
        #if defined(XL_DEBUG)
            if (_batchedIndexBuffers->Validate(locator)==0) {
                BufferDesc desc = PlatformInterface::ExtractDesc(*locator.GetUnderlying());
                assert(!(UsePooling(desc) && UseBatching(desc)));
            }
        #endif
    }

    bool ResourceSource::MarkBarrier(unsigned barrierID)
    {
        return true;
    }

    void ResourceSource::FlushDelayedReleases(unsigned gpuBarrierProgress, bool duringDestructor)
    {
        ScopedLock(_flushDelayedReleasesLock);

        const uint32 currentThread = XlGetCurrentThreadId();
        if (!duringDestructor) {
            assert(!_flushThread || _flushThread == currentThread);         // we should only be flushing from one thread
            _flushThread = currentThread;
        }

        intrusive_ptr<ResourceLocator>* delayedRelease = nullptr;
        while (_delayedReleases.try_front(delayedRelease)) {
            _delayedReleases.pop();
        }

        static unsigned callCount = 0;
        if ((callCount%30)==0) {
            _delayedReleases.compress_overflow();
            ++callCount;
        }
    }

    void ResourceSource::Tick(ThreadContext& threadContext, IManager::EventListID processedEventList, bool& deviceCreation)
    {
        _stagingBufferPool->Update(threadContext.CommandList_GetUnderConstruction());

            // ------ Defrag ------ //

        if (_batchedIndexBuffers.get()) {
            _batchedIndexBuffers->TickDefrag(threadContext, processedEventList, deviceCreation);
        }
    }

    PoolSystemMetrics   ResourceSource::CalculatePoolMetrics() const
    {
        PoolSystemMetrics result;
        result._resourcePools = _pooledGeometryBuffers->CalculateMetrics();
        result._stagingPools = _stagingBufferPool->CalculateMetrics();
        result._batchingSystemMetrics = _batchedIndexBuffers->CalculateMetrics();
        return result;
    }

    void                ResourceSource::GetQueueEvents(XlHandle waitEvents[], unsigned& waitEventsCount)
    {
        #if defined(D3D_BUFFER_UPLOAD_USE_WAITABLE_QUEUES)
            waitEvents[waitEventsCount++] = _delayedReleases.get_event();
        #endif
    }

    BufferDesc ResourceSource::AdjustDescForReusableResource(const BufferDesc& input)
    {
        BufferDesc result = input;
        if (result._type == BufferDesc::Type::LinearBuffer) {
            result._linearBufferDesc._sizeInBytes = RoundUpBufferSize(result._linearBufferDesc._sizeInBytes);
            if (false) { // CRenderer::CV_r_BufferUpload_WriteMode == WriteMode::Map) {
                result._cpuAccess = CPUAccess::Write;
            }
        }
        return result;
    }

    void ResourceSource::OnLostDevice()
    {
        FlushDelayedReleases();                  // flush all remaining delayed releases, with no limit
        
        _batchedIndexBuffers->OnLostDevice();       // (prefer calling OnLostDevice() on the batched index buffers first, because of links back to pooledGeometryBuffers)
        _pooledGeometryBuffers->OnLostDevice();
        _stagingBufferPool->OnLostDevice();
    }

    ResourceSource::ResourceSource(RenderCore::IDevice* device)
    :   _underlyingDevice(device)
    {
        _flushThread = 0;
        _frameID = 0;

        auto stagingBufferPool = std::make_shared<ResourcesPool<BufferDesc>>(device, 5*60);
        auto pooledGeometryBuffers = std::make_shared<ResourcesPool<BufferDesc>>(device);

        BufferDesc batchableIndexBuffers;
        batchableIndexBuffers._type = BufferDesc::Type::LinearBuffer;
        batchableIndexBuffers._cpuAccess = CPUAccess::Write|CPUAccess::WriteDynamic;
        batchableIndexBuffers._gpuAccess = GPUAccess::Read;
        batchableIndexBuffers._bindFlags = BindFlag::IndexBuffer;
        batchableIndexBuffers._allocationRules = AllocationRules::Pooled; // -- note that if it's pooled, there's a higher likelyhood that a resource will be turned around and reused immediately, confusing some defrag cases

        if (PlatformInterface::UseMapBasedDefrag) {
                //
                //      When using the map based defrag, we must have a non-default pool resource
                //      with read and write access specified. 
                //
                //      Without the NonVolatile flag, the 
                //      resource will end up in the Default pool on D3D9 -- which is inefficient
                //      for reading from via map. But with the NonVolatile flag, we can't set WriteDynamic,
                //      which means we can't do no-overwrite maps.
                //
                //      So we have to choose between reading back efficiently, and using no overwrite
                //      maps. The best solution would be to use WriteDynamic, but keep our own
                //      system memory mirror of these buffers (which could be used for reading from).
                //
            batchableIndexBuffers._cpuAccess = CPUAccess::Write|CPUAccess::Read|CPUAccess::WriteDynamic;
        }

        batchableIndexBuffers._linearBufferDesc._sizeInBytes = 256 * 1024;
        XlCopyNString(batchableIndexBuffers._name, "BatchedBuffer", 13);
        batchableIndexBuffers._name[13] = '\0';

        auto batchedIndexBuffers = std::make_shared<BatchedResources>(
            batchableIndexBuffers, _pooledGeometryBuffers);

        _stagingBufferPool = std::move(stagingBufferPool);
        _pooledGeometryBuffers = std::move(pooledGeometryBuffers);
        _batchedIndexBuffers = std::move(batchedIndexBuffers);
    }

    ResourceSource::~ResourceSource()
    {
        FlushDelayedReleases(~unsigned(0x0), true);     // flush all remaining delayed releases, with no limit
    }

}
