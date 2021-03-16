// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "BufferUploads_Manager.h"
#include "Metrics.h"
#include "ResourceLocator.h"
#include "PlatformInterface.h"
#include "ResourceSource.h"
#include "DataPacket.h"
#include "../RenderCore/IDevice.h"
#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/ResourceUtils.h"
#include "../RenderCore/Metal/Resource.h"
#include "../OSServices/Log.h"
#include "../OSServices/TimeUtils.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../ConsoleRig/AttachablePtr.h"
#include "../Utility/Threading/ThreadingUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/BitUtils.h"
#include <assert.h>
#include <utility>
#include <algorithm>
#include <chrono>
#include <functional>
#include "thousandeyes/futures/then.h"

#pragma warning(disable:4127)       // conditional expression is constant
#pragma warning(disable:4018)       // signed/unsigned mismatch

using namespace std::chrono_literals;

namespace BufferUploads
{
    using Box2D = RenderCore::Box2D;

                    /////////////////////////////////////////////////
                ///////////////////   M A N A G E R   ///////////////////
                    /////////////////////////////////////////////////

    static UploadDataType AsUploadDataType(const ResourceDesc& desc) 
    {
        switch (desc._type) {
        case ResourceDesc::Type::LinearBuffer:     return (desc._bindFlags&BindFlag::VertexBuffer)?(UploadDataType::Vertex):(UploadDataType::Index);
        default:
        case ResourceDesc::Type::Texture:          return UploadDataType::Texture;
        }
    }
    
        ///////////////////////////////////////////////////////////////////////////////////////////////////

    static ResourceDesc AsStagingDesc(const ResourceDesc& desc)
    {
        ResourceDesc result = desc;
        result._cpuAccess = CPUAccess::Write|CPUAccess::Read;
        result._gpuAccess = 0;
        result._bindFlags = BindFlag::TransferSrc;
        result._allocationRules |= AllocationRules::Staging;
        return result;
    }

    static ResourceDesc ApplyLODOffset(const ResourceDesc& desc, unsigned lodOffset)
    {
            //  Remove the top few LODs from the desc...
        ResourceDesc result = desc;
        if (result._type == ResourceDesc::Type::Texture) {
            result._textureDesc = RenderCore::CalculateMipMapDesc(desc._textureDesc, lodOffset);
        }
        return result;
    }

        ///////////////////////////////////////////////////////////////////////////////////////////////////

#define DEQUE_BASED_TRANSACTIONS
#define OPTIMISED_ALLOCATE_TRANSACTION

    class SimpleWakeupEvent
    {
    public:
        std::mutex _l;
        std::condition_variable _cv;
        volatile unsigned _semaphoreCount = 0;

        void Increment()
        {
            std::unique_lock<std::mutex> ul(_l);
            ++_semaphoreCount;
            _cv.notify_one();
        }
        void Wait()
        {
            std::unique_lock<std::mutex> ul(_l);
            if (!_semaphoreCount)
                _cv.wait(ul);
            _semaphoreCount = 0;
        }
    };
    
    class AssemblyLine : public std::enable_shared_from_this<AssemblyLine>
    {
    public:
        enum 
        {
            Step_PrepareStaging         = (1<<0),
            Step_TransferStagingToFinal = (1<<1),
            Step_CreateFromDataPacket   = (1<<2),
            Step_BatchingUpload         = (1<<3),
            Step_DelayedReleases        = (1<<4),
            Step_BatchedDefrag          = (1<<5)
        };
        
        // void                UpdateData(TransactionID id, IDataPacket& rawData, const PartialResource& part);
        
        TransactionMarker       Transaction_Begin(const ResourceDesc& desc, const std::shared_ptr<IDataPacket>& data, TransactionOptions::BitField flags);
        TransactionMarker       Transaction_Begin(const std::shared_ptr<IAsyncDataSource>& data, TransactionOptions::BitField flags);
        TransactionMarker       Transaction_Begin(const ResourceLocator& locator, TransactionOptions::BitField flags=0);
        void                    Transaction_AddRef(TransactionID id);
        void                    Transaction_Cancel(TransactionID id);
        void                    Transaction_Validate(TransactionID id);

        ResourceLocator         Transaction_Immediate(
                                    RenderCore::IThreadContext& threadContext,
                                    const ResourceDesc& desc, IDataPacket& data,
                                    const PartialResource&);

        bool                IsCompleted(TransactionID id, CommandList::ID lastCommandList_CommittedToImmediate);
        void                Process(unsigned stepMask, ThreadContext& context);
        ResourceLocator     GetResource(TransactionID id);

        void                Resource_Release(ResourceLocator& locator);
        void                Resource_AddRef(const ResourceLocator& locator);
        void                Resource_AddRef_IndexBuffer(const ResourceLocator& locator);
        void                Resource_Validate(const ResourceLocator& locator);

        AssemblyLineMetrics CalculateMetrics();
        PoolSystemMetrics   CalculatePoolMetrics() const;
        void                Wait(unsigned stepMask, ThreadContext& context);
        void                TriggerWakeupEvent();
        bool                QueuedWork() const;

        unsigned            FlipWritingQueueSet();
        void                OnLostDevice();

        IManager::EventListID TickResourceSource(unsigned stepMask, ThreadContext& context, bool isLoading);

        AssemblyLine(RenderCore::IDevice& device);
        ~AssemblyLine();

    protected:
        struct Transaction
        {
            uint32_t _idTopPart;
            std::atomic<unsigned> _referenceCount;
            ResourceLocator _finalResource;
            // ResourceLocator _stagingResource;
            ResourceDesc _desc;
            TimeMarker _requestTime;
            std::promise<ResourceLocator> _promise;
            std::future<void> _waitingFuture;

            std::atomic<bool> _statusLock;
            // bool _creationQueued, _stagingQueued;
            // unsigned _requestedStagingLODOffset, _actualisedStagingLODOffset;
            unsigned _retirementCommandList;
            TransactionOptions::BitField _creationOptions;
            #if defined(OPTIMISED_ALLOCATE_TRANSACTION)
                unsigned _heapIndex;
            #endif
            // int _creationFrameID;

            Transaction(unsigned idTopPart, unsigned heapIndex);
            Transaction();
            Transaction(Transaction&& moveFrom) never_throws;
            Transaction& operator=(Transaction&& moveFrom) never_throws;
            Transaction& operator=(const Transaction& cloneFrom) = delete;
        };

        #if defined(DEQUE_BASED_TRANSACTIONS)
            std::deque<Transaction>     _transactions;
            std::deque<Transaction>     _transactions_LongTerm;
        #else
            std::vector<Transaction>    _transactions;
            size_t                      _transactions_TemporaryCount, _transactions_LongTermCount;
        #endif

        #if defined(OPTIMISED_ALLOCATE_TRANSACTION)
            SimpleSpanningHeap _transactionsHeap;
            SimpleSpanningHeap _transactionsHeap_LongTerm;
        #endif

        Threading::Mutex        _transactionsLock;
        Threading::Mutex        _transactionsRepositionLock;
        std::atomic<unsigned>   _allocatedTransactionCount;
        IManager::EventListID   _transactions_resolvedEventID, _transactions_postPublishResolvedEventID;

        ResourceSource          _resourceSource;
        RenderCore::IDevice*    _device;

        Transaction*            GetTransaction(TransactionID id);
        TransactionID           AllocateTransaction(TransactionOptions::BitField flags);
        void                    ApplyRepositionEvent(ThreadContext& context, unsigned id);

        std::atomic<unsigned>   _currentQueuedBytes[(unsigned)UploadDataType::Max];
        unsigned                _nextTransactionIdTopPart;
        unsigned                _peakPrepareStaging, _peakTransferStagingToFinal, _peakCreateFromDataPacket;
        bool                    _queuedWorkFlag;
        int64_t                 _waitTime;

        #if defined(_DEBUG)
            Threading::Mutex _transactionsToBeCompletedNextFramePriorityCommit_Lock;
            std::vector<TransactionID> _transactionsToBeCompletedNextFramePriorityCommit;
        #endif

        struct PrepareStagingStep
        {
            TransactionID _id = ~TransactionID(0);
            ResourceDesc _desc;
            std::shared_ptr<IAsyncDataSource> _packet;
            PartialResource _part;
        };

        struct TransferStagingToFinalStep
        {
            TransactionID _id = ~TransactionID(0);
            ResourceLocator _stagingResource;
            PlatformInterface::StagingToFinalMapping _stagingToFinalMapping;
            unsigned _stagingByteCount;
        };

        struct CreateFromDataPacketStep
        {
            TransactionID _id = ~TransactionID(0);
            ResourceDesc _creationDesc;
            std::shared_ptr<IDataPacket> _initialisationData;
            PartialResource _part;
        };

        class QueueSet
        {
        public:
            std::queue<PrepareStagingStep> _prepareStagingSteps;
            std::queue<TransferStagingToFinalStep> _transferStagingToFinalSteps;
            std::queue<CreateFromDataPacketStep> _createFromDataPacketSteps;
            std::mutex _lock;
        };

        QueueSet _queueSet_Main;
        QueueSet _queueSet_FramePriority[4];
        unsigned _framePriority_WritingQueueSet;

        std::queue<std::function<void(AssemblyLine&, ThreadContext&)>> _queuedFunctions;
        SimpleWakeupEvent _wakeupEvent;

#if BU_BATCHING
        class BatchPreparation
        {
        public:
            std::vector<ResourceCreateStep> _batchedSteps;
            unsigned                        _batchedAllocationSize;
            BatchPreparation();
        };
        BatchPreparation _batchPreparation_Main;
#endif

        class CommandListBudget
        {
        public:
            unsigned _limit_BytesUploaded, _limit_Operations, _limit_DeviceCreates;
            CommandListBudget(bool isLoading);
        };

        // void    UpdateData_PostBackground(QueueSet&, Transaction& transaction, TransactionID id, DataPacket* rawData, const PartialResource& part);

#if BU_BATCHING
        void    ResolveBatchOperation(BatchPreparation& batchOperation, ThreadContext& context, unsigned stepMask);
#endif
        void    ReleaseTransaction(Transaction* transaction, ThreadContext& context, bool abort = false);
        void    ClientReleaseTransaction(Transaction* transaction);

        bool    Process(const CreateFromDataPacketStep& resourceCreateStep, ThreadContext& context, const CommandListBudget& budgetUnderConstruction);
        bool    Process(const PrepareStagingStep& prepareStagingStep, ThreadContext& context, const CommandListBudget& budgetUnderConstruction);
        bool    Process(const TransferStagingToFinalStep& transferStagingToFinalStep, ThreadContext& context, const CommandListBudget& budgetUnderConstruction);

        auto    ProcessQueueSet(QueueSet& queueSet, unsigned stepMask, ThreadContext& context, const CommandListBudget& budgetUnderConstruction) -> std::pair<bool,bool>;
        bool    DrainPriorityQueueSet(QueueSet& queueSet, unsigned stepMask, ThreadContext& context);

#if BU_BATCHING
        void            CopyIntoBatchedBuffer(void* destination, ResourceCreateStep* start, ResourceCreateStep* end, UnderlyingResource* resource, unsigned startOffset, unsigned offsetList[], CommandListMetrics& metricsUnderConstruction);
        static bool     SortSize_LargestToSmallest(const AssemblyLine::ResourceCreateStep& lhs, const AssemblyLine::ResourceCreateStep& rhs);
        static bool     SortSize_SmallestToLargest(const AssemblyLine::ResourceCreateStep& lhs, const AssemblyLine::ResourceCreateStep& rhs);
#endif

        auto    GetQueueSet(TransactionOptions::BitField transactionOptions) -> QueueSet &;
        void    PushStep(QueueSet&, Transaction& transaction, PrepareStagingStep&& step);
        void    PushStep(QueueSet&, Transaction& transaction, TransferStagingToFinalStep&& step);
        void    PushStep(QueueSet&, Transaction& transaction, CreateFromDataPacketStep&& step);

        void    CompleteWaitForDescFuture(TransactionID transactionID, std::future<ResourceDesc> descFuture, const std::shared_ptr<IAsyncDataSource>& data, PartialResource part);
        void    CompleteWaitForDataFuture(TransactionID transactionID, std::future<void> prepareFuture, const ResourceLocator& stagingResource, const PlatformInterface::StagingToFinalMapping& stagingToFinalMapping, unsigned stagingByteCount);
    };

    static PartialResource PartialResource_All()
    {
        return PartialResource{};
    }

    TransactionMarker AssemblyLine::Transaction_Begin(
        const ResourceDesc& desc, const std::shared_ptr<IDataPacket>& data, TransactionOptions::BitField flags)
    {
        assert(desc._name[0]);
        
        TransactionID transactionID = AllocateTransaction(flags);
        Transaction* transaction = GetTransaction(transactionID);
        assert(transaction);
        transaction->_desc = desc;
        
        /*
            todo -- do this in the actual CreateFromDataPacketStep step instead
        const bool allowInitialisationOnConstruction = false;
        if (!allowInitialisationOnConstruction) {
            // We need to make sure the "TransferDst" flag is set for the transaction -- because we know we will receive a 
            // transfer in this case.
            transaction->_desc._bindFlags |= BindFlag::TransferDst;
        }
        */

        // transaction->_creationQueued = true;    // thread safe, because it's not possible to have operations queued yet

        #if defined(_DEBUG)
                    //
                    //      Validate the size of information in the initialisation packet.
                    //
            if (data && desc._type == ResourceDesc::Type::Texture) {
                for (unsigned m=0; m<desc._textureDesc._mipCount; ++m) {
                    const size_t dataSize = data->GetDataSize(SubResourceId{m, 0});
                    if (dataSize) {
                        TextureDesc mipMapDesc     = RenderCore::CalculateMipMapDesc(desc._textureDesc, m);
                        mipMapDesc._mipCount       = 1;
                        const size_t expectedSize  = RenderCore::ByteCount(mipMapDesc);
                        assert(std::max(size_t(16),dataSize) == std::max(size_t(16),expectedSize));
                    }
                }
            }
        #endif

            //
            //      Have to increase _currentQueuedBytes before we push in the create step... Otherwise the create 
            //      step can actually happen first, causing _currentQueuedBytes to actually go negative! it actually
            //      happens frequently enough to create blips in the graph.
            //  
        _currentQueuedBytes[(unsigned)AsUploadDataType(desc)] += RenderCore::ByteCount(desc);

        PushStep(
            GetQueueSet(flags),
            *transaction,
            CreateFromDataPacketStep { transactionID, desc, data, PartialResource_All() });

        return { transaction->_promise.get_future(), transactionID };
    }

    TransactionMarker AssemblyLine::Transaction_Begin(
        const std::shared_ptr<IAsyncDataSource>& data, TransactionOptions::BitField flags)
    {
        TransactionID transactionID = AllocateTransaction(flags);
        Transaction* transaction = GetTransaction(transactionID);
        assert(transaction);

        TransactionMarker result { transaction->_promise.get_future(), transactionID };

        // Let's optimize the case where the desc is available immediately, since certain
        // usage patterns will allow for that
        auto descFuture = data->GetDesc();
        auto status = descFuture.wait_for(0s);
        if (status == std::future_status::ready) {

            CompleteWaitForDescFuture(transactionID, std::move(descFuture), data, PartialResource_All());

        } else {
            ++transaction->_referenceCount;

            auto weakThis = weak_from_this();
            assert(!transaction->_waitingFuture.valid());
            transaction->_waitingFuture = thousandeyes::futures::then(
                std::move(descFuture),
                [weakThis, transactionID, data](std::future<ResourceDesc> completedFuture) {
                    auto t = weakThis.lock();
                    if (!t)
                        Throw(std::runtime_error("Assembly line was destroyed before future completed"));

                    t->CompleteWaitForDescFuture(transactionID, std::move(completedFuture), data, PartialResource_All());
                });
        }

        return result;
    }

    TransactionMarker   AssemblyLine::Transaction_Begin(const ResourceLocator& locator, TransactionOptions::BitField flags)
    {
        ResourceDesc desc = locator._resource->GetDesc();
        if (desc._type == ResourceDesc::Type::Texture) {
            assert(desc._textureDesc._mipCount <= (IntegerLog2(std::max(desc._textureDesc._width, desc._textureDesc._height))+1));
        }
        /*if (locator->Size() != ~unsigned(0x0) && locator->Size() != 0) {
            // assert(desc._type == ResourceDesc::Type::LinearBuffer);
            if (desc._type == ResourceDesc::Type::LinearBuffer) {
                desc._linearBufferDesc._sizeInBytes = locator->Size();
            }
        }*/

        const BatchedResources::ResultFlags::BitField batchFlags = _resourceSource.IsBatchedResource(locator, desc); (void)batchFlags;
        // const bool isPooled = !!(batchFlags & BatchedResources::ResultFlags::IsBatched);
        // if (isPooled) {          (the tighter test doesn't seem to work here... We need a "IsBatchedCandidate"
        if (desc._bindFlags & BindFlag::IndexBuffer) {
            desc._allocationRules |= AllocationRules::Pooled|AllocationRules::Batched;
        }

        // flags |= TransactionOptions::FramePriority;
        assert(desc._type != ResourceDesc::Type::Unknown);
        assert(batchFlags != BatchedResources::ResultFlags::IsCurrentlyDefragging);

            //
            //      Note -- "existingResource" should not be part of another transaction. Let's check to make sure
            //          We should also check to make sure "desc" and "existingResource" match...
            //
            //          (in the case of batched resources, this can happen often)
            //
        #if 0 && defined(_DEBUG)      // DavidJ -- need to have multiple transactions for a single resource for terrain texture set!
            {
                const bool mightBeBatched = !!(initDataDesc._bindFlags & BindFlag::IndexBuffer);
                if (!mightBeBatched) {
                    std::deque<Transaction>::iterator endi=_transactions.end();
                    for (std::deque<Transaction>::iterator i=_transactions.begin(); i!=endi; ++i) {
                        if (i->_finalResource.get()==locator._resource) {
                            assert(!(i->_referenceCount>>24));      // we can have 2 transactions for the same resource sometimes... but when this happens, there must be no client lock on the first resource. So long as we complete in order, we should get the right result (though perhaps the first upload become redundant)
                        }
                    }
                }
            }
        #endif

        TransactionID result = AllocateTransaction(flags);
        Transaction* transaction = GetTransaction(result);
        assert(transaction);
        transaction->_desc = desc;
        transaction->_finalResource = locator;
        // transaction->_creationQueued = true;
        // transaction->_creationFrameID = PlatformInterface::GetFrameID();
        return { std::future<ResourceLocator>{}, result };
    }

    void AssemblyLine::ReleaseTransaction(Transaction* transaction, ThreadContext& context, bool abort)
    {
        AssemblyLineRetirement retirementBuffer;
        AssemblyLineRetirement* retirement = &retirementBuffer;
        CommandListMetrics& metrics = context.GetMetricsUnderConstruction();
        if ((metrics._retirementCount+1) <= dimof(metrics._retirements)) {
            retirement = &metrics._retirements[metrics._retirementCount];
        }
            
            //
            //      We still have to do this before doing the ref count decrement.
            //      This is because we can decrement the reference count here, then the client might release it's
            //      lock shortly afterwards in another thread. The other thread might then clear out the transaction
            //      in ClientReleaseTransaction()
            //
        retirement->_desc = transaction->_desc;
        retirement->_requestTime = transaction->_requestTime;

        auto newRefCount = --transaction->_referenceCount;
        assert(newRefCount>=0);

        if ((newRefCount&0x00ffffff)==0) {
                //      After the last system reference is released (regardless of client references) we call it retired...
            transaction->_retirementCommandList = abort ? 0 : context.CommandList_GetUnderConstruction();
            retirement->_retirementTime = OSServices::GetPerformanceCounter();
            // assert((retirement->_retirementTime - retirement->_requestTime)<100000000);      this just tends to happen while debugging!
            if ((metrics._retirementCount+1) <= dimof(metrics._retirements)) {
                metrics._retirementCount++;
            } else {
                metrics._retirementsOverflow.push_back(*retirement);
            }
        }

        if (newRefCount<=0) {
            transaction->_finalResource = {};

            #if defined(OPTIMISED_ALLOCATE_TRANSACTION)
                bool isLongTerm      = !!(transaction->_creationOptions & TransactionOptions::LongTerm);
                unsigned heapIndex   = transaction->_heapIndex;
            #endif

                //
                //      This is a destroy event... actually we don't need to do anything.
                //      it's already considered destroyed because the ref count is 0.
                //      But let's clear out the members, anyway. This will also free the textures (if they need freeing)
                //
            *transaction = Transaction();
            transaction->_referenceCount.store(~0x0u);    // set reference count to invalid value to signal that it's ok to reuse now. Note that this has to come after all other work has completed
            --_allocatedTransactionCount;

            #if defined(OPTIMISED_ALLOCATE_TRANSACTION)
                if (isLongTerm) {
                    _transactionsHeap_LongTerm.Deallocate(heapIndex<<4, 1<<4);
                } else {
                    _transactionsHeap.Deallocate(heapIndex<<4, 1<<4);
                }
            #endif
        }
    }

    void AssemblyLine::ClientReleaseTransaction(Transaction* transaction)
    {
        auto newRefCount = (transaction->_referenceCount -= 0x01000000);
        assert(newRefCount>=0);
        if (newRefCount<=0) {
            transaction->_finalResource = {};

            #if defined(OPTIMISED_ALLOCATE_TRANSACTION)
                bool isLongTerm      = !!(transaction->_creationOptions & TransactionOptions::LongTerm);
                unsigned heapIndex   = transaction->_heapIndex;
            #endif

            *transaction = Transaction();
            transaction->_referenceCount.store(~0x0u);
            --_allocatedTransactionCount;

            #if defined(OPTIMISED_ALLOCATE_TRANSACTION)
                if (isLongTerm) {
                    _transactionsHeap_LongTerm.Deallocate(heapIndex<<4, 1<<4);
                } else {
                    _transactionsHeap.Deallocate(heapIndex<<4, 1<<4);
                }
            #endif
        }
    }

    void AssemblyLine::Transaction_Cancel(TransactionID id)
    {
        Transaction* transaction = GetTransaction(id);
        assert(transaction);
        if (transaction) {
            ClientReleaseTransaction(transaction);    // release the client ref count
        }
    }

    static bool IsFull2DPlane(const ResourceDesc& resDesc, const RenderCore::Box2D& box)
    {
        assert(resDesc._type == ResourceDesc::Type::Texture);
        if (box == Box2D{}) return true;
        return 
            box._left == 0 && box._top == 0
            && box._right == resDesc._textureDesc._width
            && box._left == resDesc._textureDesc._height;
    }

    static bool IsAllLodLevels(const ResourceDesc& resDesc, unsigned lodLevelMin, unsigned lodLevelMax)
    {
        assert(resDesc._type == ResourceDesc::Type::Texture);
        assert(lodLevelMin != lodLevelMax);
        auto max = std::min(lodLevelMax, (unsigned)resDesc._textureDesc._mipCount-1);
        return (lodLevelMin == 0 && max == resDesc._textureDesc._mipCount-1);
    }

    static bool IsAllArrayLayers(const ResourceDesc& resDesc, unsigned arrayLayerMin, unsigned arrayLayerMax)
    {
        assert(resDesc._type == ResourceDesc::Type::Texture);
        assert(arrayLayerMin != arrayLayerMax);
        if (resDesc._textureDesc._arrayCount == 0) return true;

        auto max = std::min(arrayLayerMax, (unsigned)resDesc._textureDesc._arrayCount-1);
        return (arrayLayerMin == 0 && max == resDesc._textureDesc._arrayCount-1);
    }

    static std::pair<ResourceDesc, PlatformInterface::StagingToFinalMapping> CalculatePartialStagingDesc(
        const ResourceDesc& dstDesc,
        const PartialResource& part)
    {
        assert(dstDesc._type == ResourceDesc::Type::Texture);
        ResourceDesc stagingDesc = AsStagingDesc(dstDesc);
        PlatformInterface::StagingToFinalMapping mapping;
        mapping._dstBox = part._box;
        if (IsFull2DPlane(dstDesc, mapping._dstBox)) {
            // When writing to the full 2d plane, we can selectively update only some lod levels
            if (!IsAllLodLevels(dstDesc, part._lodLevelMin, part._lodLevelMax)) {
                mapping._stagingLODOffset = part._lodLevelMin;
                mapping._dstLodLevelMin = part._lodLevelMin;
                mapping._dstLodLevelMax = std::min(part._lodLevelMax, (unsigned)dstDesc._textureDesc._mipCount-1);
                stagingDesc = ApplyLODOffset(stagingDesc, mapping._stagingLODOffset);
            }
        } else {
            // We need this restriction because otherwise (assuming the mip chain goes to 1x1) we
            // would have to recalculate all mips
            if (!IsAllLodLevels(dstDesc, part._lodLevelMin, part._lodLevelMax))
                Throw(std::runtime_error("When updating texture data for only part of the 2d plane, you must update all lod levels"));

            // Shrink the size of the staging texture to just the parts we want
            assert(mapping._dstBox._right > mapping._dstBox._left);
            assert(mapping._dstBox._bottom > mapping._dstBox._top);
            mapping._stagingXYOffset = { (unsigned)mapping._dstBox._left, (unsigned)mapping._dstBox._top };
            stagingDesc._textureDesc._width = mapping._dstBox._right - mapping._dstBox._left;
            stagingDesc._textureDesc._height = mapping._dstBox._bottom - mapping._dstBox._top;
        }

        if (!IsAllArrayLayers(dstDesc, part._arrayIndexMin, part._arrayIndexMax)) {
            assert(part._arrayIndexMax > part._arrayIndexMin);
            mapping._stagingArrayOffset = part._arrayIndexMin;
            mapping._dstArrayLayerMin = part._arrayIndexMin;
            mapping._dstArrayLayerMax = std::min(part._arrayIndexMax, (unsigned)dstDesc._textureDesc._arrayCount-1);
            stagingDesc._textureDesc._arrayCount = mapping._dstArrayLayerMax + 1 - mapping._dstArrayLayerMin;
            if (stagingDesc._textureDesc._arrayCount == 1)
                stagingDesc._textureDesc._arrayCount = 0;
        }

        return std::make_pair(stagingDesc, mapping);
    }

    ResourceLocator AssemblyLine::Transaction_Immediate(
        RenderCore::IThreadContext& threadContext,
        const ResourceDesc& descInit, IDataPacket& initialisationData,
        const PartialResource& partInit)
    {
        PartialResource part = partInit;
        ResourceDesc desc = descInit;

        unsigned requestedStagingLODOffset = 0;
        if (desc._type == ResourceDesc::Type::Texture) {
            unsigned maxLodOffset = IntegerLog2(std::min(desc._textureDesc._width, desc._textureDesc._height))-2;
            requestedStagingLODOffset = std::min(part._lodLevelMin, maxLodOffset);
        }
    
        auto finalResourceConstruction = _resourceSource.Create(
            desc, &initialisationData, ResourceSource::CreationOptions::AllowDeviceCreation);
        if (!finalResourceConstruction._locator._resource)
            return {};
    
        if (!(finalResourceConstruction._flags & ResourceSource::ResourceConstruction::Flags::InitialisationSuccessful)) {
            
            ResourceDesc stagingDesc;
            PlatformInterface::StagingToFinalMapping stagingToFinalMapping;
            std::tie(stagingDesc, stagingToFinalMapping) = CalculatePartialStagingDesc(desc, part);

            auto stagingConstruction = _resourceSource.Create(
                stagingDesc, &initialisationData, ResourceSource::CreationOptions::AllowDeviceCreation);
            assert(stagingConstruction._locator._resource);
            if (!stagingConstruction._locator._resource)
                return {};
    
            PlatformInterface::UnderlyingDeviceContext deviceContext(threadContext);
            deviceContext.WriteToTextureViaMap(
                *stagingConstruction._locator._resource,
                stagingDesc, Box2D(),
                [&part, &initialisationData](RenderCore::SubResourceId sr) -> RenderCore::SubResourceInitData
                {
                    RenderCore::SubResourceInitData result = {};
                    auto size = initialisationData.GetDataSize(SubResourceId{sr._mip, sr._arrayLayer});
                    const void* data = initialisationData.GetData(SubResourceId{sr._mip, sr._arrayLayer});
                    assert(data);
					result._data = MakeIteratorRange(data, PtrAdd(data, size));
                    result._pitches = initialisationData.GetPitches(SubResourceId{sr._mip, sr._arrayLayer});
                    return result;
                });
    
            deviceContext.UpdateFinalResourceFromStaging(
                *finalResourceConstruction._locator._resource, 
                *stagingConstruction._locator._resource, desc, 
                stagingToFinalMapping);
        }
    
        return finalResourceConstruction._locator;
    }

    void AssemblyLine::Transaction_Validate(TransactionID id)
    {
        #if defined(_DEBUG)
            Transaction* transaction = GetTransaction(id);
            assert(transaction);
            if (transaction) {
                    //  make sure this transaction will be complete in time to use 
                    //  for the the next render frame
                if (!transaction->_finalResource._resource) {
                    assert(transaction->_creationOptions & TransactionOptions::FramePriority);
                }
                ScopedLock(_transactionsToBeCompletedNextFramePriorityCommit_Lock);
                _transactionsToBeCompletedNextFramePriorityCommit.push_back(id);
            }
        #endif
    }

    void AssemblyLine::Transaction_AddRef(TransactionID id)
    {
        Transaction* transaction = GetTransaction(id);
        assert(transaction);
        if (transaction) {
            transaction->_referenceCount += 0x01000000;
        }
    }

    bool AssemblyLine::IsCompleted(TransactionID id, CommandList::ID lastCommandList_CommittedToImmediate)
    {
        Transaction* transaction = GetTransaction(id);
        assert(transaction);
        if (transaction) {
            auto referenceCount = transaction->_referenceCount.load();
                // note --  This must return the frame index for the current thread (if there are threads working on
                //          different frames). 
            // const int currentRenderThreadFrameId = PlatformInterface::GetFrameID(); 
            // return  ((referenceCount & 0x00ffffff) == 0)
            //     &&  (transaction->_retirementCommandList <= lastCommandList_CommittedToImmediate)
            //     &&  (transaction->_creationFrameID <= currentRenderThreadFrameId)       // prevent the transaction from completing on a frame earlier than it's creation
            //     ;
            const bool isCompleted = 
                ((referenceCount & 0x00ffffff) == 0)
                &&  (transaction->_retirementCommandList <= lastCommandList_CommittedToImmediate)
                ;
            return isCompleted;
        } else {
            return false;
        }
    }

    bool AssemblyLine::QueuedWork() const
    {
        return _queuedWorkFlag;
    }

        //////////////////////////////////////////////////////////////////////////////////////////////

    AssemblyLine::Transaction::Transaction(unsigned idTopPart, unsigned heapIndex)
    {
        _idTopPart = idTopPart;
        _statusLock = 0;
        // _creationQueued = _stagingQueued = false;
        _referenceCount = 0;
        // _requestedStagingLODOffset = 0;
        // _actualisedStagingLODOffset = ~unsigned(0x0);
        _retirementCommandList = ~unsigned(0x0);
        _creationOptions = 0;
        // _creationFrameID = 0;
        #if defined(OPTIMISED_ALLOCATE_TRANSACTION)
            _heapIndex = heapIndex;
        #endif
    }

    AssemblyLine::Transaction::Transaction()
    {
        _idTopPart = 0;
        _statusLock = 0;
        // _creationQueued = _stagingQueued = false;
        _referenceCount = 0;
        // _requestedStagingLODOffset = 0;
        // _actualisedStagingLODOffset = ~unsigned(0x0);
        _retirementCommandList = ~unsigned(0x0);
        _creationOptions = 0;
        // _creationFrameID = 0;
        #if defined(OPTIMISED_ALLOCATE_TRANSACTION)
            _heapIndex = ~unsigned(0x0);
        #endif
    }

    AssemblyLine::Transaction::Transaction(Transaction&& moveFrom) never_throws
    {
        _referenceCount = 0;
        _statusLock = 0;

        _idTopPart = moveFrom._idTopPart;
        _finalResource = std::move(moveFrom._finalResource);
        // _stagingResource = std::move(moveFrom._stagingResource);
        _desc = moveFrom._desc;
        _requestTime = moveFrom._requestTime;
        _promise = std::move(moveFrom._promise);
        _waitingFuture = std::move(moveFrom._waitingFuture);

        // _creationQueued = moveFrom._creationQueued;
        // _stagingQueued = moveFrom._stagingQueued;
        // _requestedStagingLODOffset = moveFrom._requestedStagingLODOffset;
        // _actualisedStagingLODOffset = moveFrom._actualisedStagingLODOffset;
        _retirementCommandList = moveFrom._retirementCommandList;
        _creationOptions = moveFrom._creationOptions;
        #if defined(OPTIMISED_ALLOCATE_TRANSACTION)
            _heapIndex = moveFrom._heapIndex;
        #endif
        // _creationFrameID = moveFrom._creationFrameID;

        moveFrom._idTopPart = 0;
        moveFrom._statusLock = 0;
        moveFrom._referenceCount = 0;
        // moveFrom._requestedStagingLODOffset = 0;
        // moveFrom._actualisedStagingLODOffset = ~unsigned(0x0);
        moveFrom._retirementCommandList = ~unsigned(0x0);
        moveFrom._creationOptions = 0;
        #if defined(OPTIMISED_ALLOCATE_TRANSACTION)
            moveFrom._heapIndex = ~unsigned(0x0);
        #endif
    }

    auto AssemblyLine::Transaction::operator=(Transaction&& moveFrom) never_throws -> Transaction&
    {
        for (;;) {
            bool expected = false;
            if (_statusLock.compare_exchange_strong(expected, true)) break;
            Threading::Pause();
        }

        _idTopPart = moveFrom._idTopPart;
        _finalResource = std::move(moveFrom._finalResource);
        // _stagingResource = std::move(moveFrom._stagingResource);
        _desc = moveFrom._desc;
        _requestTime = moveFrom._requestTime;
        _promise = std::move(moveFrom._promise);
        _waitingFuture = std::move(moveFrom._waitingFuture);

        // _creationQueued = moveFrom._creationQueued;
        // _stagingQueued = moveFrom._stagingQueued;
        // _requestedStagingLODOffset = moveFrom._requestedStagingLODOffset;
        // _actualisedStagingLODOffset = moveFrom._actualisedStagingLODOffset;
        _retirementCommandList = moveFrom._retirementCommandList;
        _creationOptions = moveFrom._creationOptions;
        #if defined(OPTIMISED_ALLOCATE_TRANSACTION)
            _heapIndex = moveFrom._heapIndex;
        #endif
        // _creationFrameID = moveFrom._creationFrameID;

        moveFrom._idTopPart = 0;
        moveFrom._statusLock = 0;
        moveFrom._referenceCount = 0;
        // moveFrom._requestedStagingLODOffset = 0;
        // moveFrom._actualisedStagingLODOffset = ~unsigned(0x0);
        moveFrom._retirementCommandList = ~unsigned(0x0);
        moveFrom._creationOptions = 0;
        #if defined(OPTIMISED_ALLOCATE_TRANSACTION)
            moveFrom._heapIndex = ~unsigned(0x0);
        #endif

        auto lockRelease = _statusLock.exchange(false);
        assert(lockRelease==1); (void)lockRelease;

            // note that reference counts are unaffected here!
            // the reference count for "this" and "moveFrom" don't change

        return *this;
    }

    AssemblyLine::AssemblyLine(RenderCore::IDevice& device)
    :   _resourceSource(device)
    ,   _device(&device)
    #if defined(OPTIMISED_ALLOCATE_TRANSACTION)
        ,   _transactionsHeap((2*1024)<<4)
        ,   _transactionsHeap_LongTerm(512<<4)
    #endif
    {
        _nextTransactionIdTopPart = 64;
        #if defined(DEQUE_BASED_TRANSACTIONS)
            _transactions.resize(2*1024);
            _transactions_LongTerm.resize(512);
            for (auto i=_transactions.begin(); i!=_transactions.end(); ++i)
                i->_referenceCount.store(~0x0u);
            for (auto i=_transactions_LongTerm.begin(); i!=_transactions_LongTerm.end(); ++i)
                i->_referenceCount.store(~0x0u);
        #else
            _transactions.resize(6*1024);
            for (auto i=_transactions.begin(); i!=_transactions.end(); ++i)
                i->_referenceCount.store(~0x0u);
            _transactions_TemporaryCount = _transactions_LongTermCount = 0;
        #endif
        _peakPrepareStaging = _peakTransferStagingToFinal =_peakCreateFromDataPacket = 0;
        _allocatedTransactionCount = 0;
        _queuedWorkFlag = false;
        XlZeroMemory(_currentQueuedBytes);
        _transactions_resolvedEventID = _transactions_postPublishResolvedEventID = 0;
        _framePriority_WritingQueueSet = 0;
    }

    AssemblyLine::~AssemblyLine()
    {
    }

    TransactionID AssemblyLine::AllocateTransaction(TransactionOptions::BitField flags)
    {
            //  Note; some of the vector code here is not thread safe... We can't have 
            //      two threads in AllocateTransaction at the same time. Let's just use a mutex.
        ScopedLock(_transactionsLock);

        TransactionID result;
        uint32_t idTopPart = _nextTransactionIdTopPart++;

        #if defined(OPTIMISED_ALLOCATE_TRANSACTION)

            bool isLongTerm = !!(flags & TransactionOptions::LongTerm);
            auto& spanningHeap = isLongTerm ? _transactionsHeap_LongTerm : _transactionsHeap;
            auto& transactions = isLongTerm ? _transactions_LongTerm : _transactions;

            if (spanningHeap.CalculateHeapSize() + (1<<4) > 0xffff)
                Throw(::Exceptions::BasicLabel("Buffer uploads spanning heap reached maximium size. Aborting transaction."));

            result = spanningHeap.Allocate(1<<4);
            if (result == ~unsigned(0x0)) {
                result = spanningHeap.AppendNewBlock(1<<4);
            }

            result >>= 4;
            if (result >= transactions.size()) {
                transactions.resize((unsigned int)(result+1));
            }
            auto destinationPosition = transactions.begin() + ptrdiff_t(result);
            result |= (uint64_t(idTopPart)<<32) | (uint64_t(isLongTerm)<<63ull);

        #elif defined(DEQUE_BASED_TRANSACTIONS)

            std::deque<Transaction>::iterator destinationPosition = _transactions.end();
            if (flags & TransactionOptions::LongTerm) {
                std::deque<Transaction>::iterator endi=_transactions_LongTerm.end();
                for (std::deque<Transaction>::iterator i=_transactions_LongTerm.begin(); i!=endi; ++i) {
                    if (i->_referenceCount.load() == ~0x0u) {
                        destinationPosition = i;
                        break;
                    }
                }

                //  If we didn't find a free space, we can try to allocate a new one...
                if (destinationPosition == _transactions.end()) {
                    _transactions_LongTerm.push_back(Transaction());
                    destinationPosition = _transactions_LongTerm.end()-1;
                }

                result = (std::distance(_transactions_LongTerm.begin(), destinationPosition)) | (UINT64(idTopPart)<<32) | (1ull<<63ull);
            } else {
                std::deque<Transaction>::iterator endi=_transactions.end();
                for (std::deque<Transaction>::iterator i=_transactions.begin(); i!=endi; ++i) {
                    if (i->_referenceCount.load() == ~0x0u) {
                        destinationPosition = i;
                        break;
                    }
                }

                    //  If we didn't find a free space, we can try to allocate a new one...
                if (destinationPosition == _transactions.end()) {
                    _transactions.push_back(Transaction());
                    destinationPosition = _transactions.end()-1;
                }

                result = (std::distance(_transactions.begin(), destinationPosition)) | (UINT64(idTopPart)<<32);
            }

        #else

            std::vector<Transaction>::iterator destinationPosition = _transactions.end();
            if (flags & TransactionOptions::LongTerm) {
                std::vector<Transaction>::reverse_iterator endi=_transactions.rbegin() + _transactions_LongTermCount;
                for (std::vector<Transaction>::reverse_iterator i=_transactions.rbegin(); i!=endi; ++i) {
                    if (i->_referenceCount.load() == ~0x0u) {
                        destinationPosition = i.base();
                        break;
                    }
                }
            } else {
                std::vector<Transaction>::iterator endi=_transactions.begin() + _transactions_TemporaryCount;
                for (std::vector<Transaction>::iterator i=_transactions.begin(); i!=endi; ++i) {
                    if (i->_referenceCount.load() == ~0x0u) {
                        destinationPosition = i;
                        break;
                    }
                }
            }

                //  If we didn't find a free space, we can try to allocate a new one...
            if (destinationPosition == _transactions.end()) {
                    //      We can't reallocate or reorder _transactions, because other threads may be using it. If
                    //      we run out of space, we need to fail, and wait to try again.
                if ((_transactions_LongTermCount+_transactions_TemporaryCount+1) <= _transactions.size()) {
                    size_t index;
                    if (flags & TransactionOptions::LongTerm) {
                        index = _transactions.size() - (++_transactions_LongTermCount);
                    } else {
                        index = _transactions_TemporaryCount++;
                    }
                    destinationPosition = _transactions.begin()+index;
                } else {
                    return 0;   // no room left, we've allocated too many transactions!
                }
            }

            result = (std::distance(_transactions.begin(), destinationPosition))|(UINT64(idTopPart)<<32);
            assert(destinationPosition->_referenceCount==-1);

        #endif

        Transaction newTransaction(idTopPart, uint32_t(result));
        newTransaction._requestTime = OSServices::GetPerformanceCounter();
        newTransaction._creationOptions = flags;

            // Start with a client ref count 1
        destinationPosition->_referenceCount.store(0x01000000);
        ++_allocatedTransactionCount;

        *destinationPosition = std::move(newTransaction);

        return result;
    }

    AssemblyLine::Transaction* AssemblyLine::GetTransaction(TransactionID id)
    {
        unsigned index = unsigned(id);
        unsigned key = unsigned(id>>32) & ~(1<<31);
        #if defined(DEQUE_BASED_TRANSACTIONS)
            ScopedLock(_transactionsLock);       // must be locked when using the deque method... if the deque is resized at the same time, operator[] can seem to fail
            if (id & (1ull<<63ull)) {
                if ((index < _transactions_LongTerm.size()) && (key == _transactions_LongTerm[index]._idTopPart)) {
                    return &_transactions_LongTerm[index];
                }
            } else {
                if ((index < _transactions.size()) && (key == _transactions[index]._idTopPart)) {
                    return &_transactions[index];
                }
            }
        #else
            if (index < _transactions.size()) {
                if (key == _transactions[index]._idTopPart) {
                    return &_transactions[index];
                }
            }
        #endif
        return NULL;
    }

    void AssemblyLine::Wait(unsigned stepMask, ThreadContext& context)
    {
        int64_t startTime = OSServices::GetPerformanceCounter();

#if 0
        #if defined(D3D_BUFFER_UPLOAD_USE_WAITABLE_QUEUES)

            const unsigned queueSetCount = 1+dimof(_queueSet_FramePriority);
            XlHandle waitEvents[4]; // +queueSetCount*4];
            unsigned waitEventsCount = 0;

            /*if (stepMask & Step_CreateResource)             { waitEvents[waitEventsCount++] = _queueSet_Main._resourceCreateSteps.get_event(); }
            if (stepMask & Step_CreateStagingBuffer)        { waitEvents[waitEventsCount++] = _queueSet_Main._stagingBufferCreateSteps.get_event(); }
            if (stepMask & Step_UploadData)                 { waitEvents[waitEventsCount++] = _queueSet_Main._uploadSteps.get_event(); }
            if (stepMask & Step_PrepareData)                { waitEvents[waitEventsCount++] = _queueSet_Main._prepareSteps.get_event(); }
            for (unsigned c=0; c<dimof(_queueSet_FramePriority); ++c) {
                if (stepMask & Step_CreateResource)         { waitEvents[waitEventsCount++] = _queueSet_FramePriority[c]._resourceCreateSteps.get_event(); }
                if (stepMask & Step_CreateStagingBuffer)    { waitEvents[waitEventsCount++] = _queueSet_FramePriority[c]._stagingBufferCreateSteps.get_event(); }
                if (stepMask & Step_UploadData)             { waitEvents[waitEventsCount++] = _queueSet_FramePriority[c]._uploadSteps.get_event(); }
                if (stepMask & Step_PrepareData)            { waitEvents[waitEventsCount++] = _queueSet_FramePriority[c]._prepareSteps.get_event(); }
            }*/
            waitEvents[waitEventsCount++] = context.GetWakeupEvent();

            if (stepMask & Step_DelayedReleases) {
                _resourceSource.GetQueueEvents(waitEvents, waitEventsCount);
            }
            if (extraWaitHandle && extraWaitHandle!=XlHandle_Invalid) {
                waitEvents[waitEventsCount++] = extraWaitHandle;
            }
            const unsigned timeout = 1000;     // We have to timeout frequently to check if it's time to do a command list resolve
            XlWaitForMultipleSyncObjects(waitEventsCount, waitEvents, false, timeout, false);

                //  Note -- Because we're waiting for multiple objects, many might be signaled at the same time.
                //          When this happens, windows will awake on the first one, and probably just reset just the 
                //          first one. But, we'll go ahead and process all 

        #else

            Threading::YieldTimeSlice();

        #endif
#endif
        _wakeupEvent.Wait();

        CommandListMetrics& metricsUnderConstruction = context.GetMetricsUnderConstruction();
        metricsUnderConstruction._waitTime += OSServices::GetPerformanceCounter() - startTime;
        metricsUnderConstruction._wakeCount++;
    }

    void AssemblyLine::TriggerWakeupEvent()
    {
        _wakeupEvent.Increment();
    }

#if BU_BATCHING
    bool AssemblyLine::SortSize_LargestToSmallest(const AssemblyLine::ResourceCreateStep& lhs, const AssemblyLine::ResourceCreateStep& rhs)     { return RenderCore::ByteCount(lhs._creationDesc) > RenderCore::ByteCount(rhs._creationDesc); }
    bool AssemblyLine::SortSize_SmallestToLargest(const AssemblyLine::ResourceCreateStep& lhs, const AssemblyLine::ResourceCreateStep& rhs)     { return RenderCore::ByteCount(lhs._creationDesc) < RenderCore::ByteCount(rhs._creationDesc); }

    void AssemblyLine::CopyIntoBatchedBuffer(   
        void* destination, ResourceCreateStep* start, ResourceCreateStep* end,
        UnderlyingResource* resource, unsigned startOffset, unsigned offsetList[], 
        CommandListMetrics& metricsUnderConstruction)
    {
        unsigned queuedBytesAdjustment[dimof(_currentQueuedBytes)];
        XlZeroMemory(queuedBytesAdjustment);

        unsigned offset = startOffset;
        unsigned* offsetWriteIterator=offsetList;
        for (ResourceCreateStep* i=start; i!=end; ++i, ++offsetWriteIterator) {
            Transaction* transaction = GetTransaction(i->_id);
            assert(transaction);
            unsigned size = RenderCore::ByteCount(transaction->_desc);
            const void* sourceData = i->_initialisationData?i->_initialisationData->GetData():NULL;
            if (sourceData && destination) {
                assert(size == i->_initialisationData->GetDataSize());
                XlCopyMemoryAlign16(PtrAdd(destination, offset), sourceData, size);
            }
            (*offsetWriteIterator) = offset;
            queuedBytesAdjustment[(unsigned)AsUploadDataType(transaction->_desc)] -= size;
            offset += MarkerHeap<uint16_t>::AlignSize(size);
        }

        for (unsigned c=0; c<dimof(queuedBytesAdjustment); ++c) {
            _currentQueuedBytes[c] += queuedBytesAdjustment[c];
        }
    }

    static unsigned ResolveOffsetValue(unsigned inputOffset, unsigned size, const std::vector<DefragStep>& steps)
    {
        for (std::vector<DefragStep>::const_iterator i=steps.begin(); i!=steps.end(); ++i) {
            if (inputOffset >= i->_sourceStart && inputOffset < i->_sourceEnd) {
                assert((inputOffset+size) <= i->_sourceEnd);
                return inputOffset + i->_destination - i->_sourceStart;
            }
        }
        assert(0);
        return inputOffset;
    }

    void AssemblyLine::ApplyRepositionEvent(ThreadContext& context, unsigned id)
    {
            //
            //      We need to prevent GetTransaction from returning a partial result while this is occuring
            //      Since we modify both transaction._finalResource & transaction._resourceOffsetValue, it's
            //      possible that another thread could get the update of one, but not the other. So we have
            //      to lock. It might be ok if we went through and cleared all of the _finalResource values
            //      of the transactions we're going to change first -- but there's still a tiny chance that
            //      that method would fail.
            //
        ScopedLock(_transactionsRepositionLock);

        Event_ResourceReposition* begin = NULL, *end = NULL;
        context.EventList_Get(id, begin, end);
        #if defined(DEQUE_BASED_TRANSACTIONS)
            const size_t temporaryCount = _transactions.size();
            const size_t longTermCount = _transactions_LongTerm.size();
        #else
            const size_t temporaryCount = _transactions_TemporaryCount;
            const size_t longTermCount = _transactions_LongTermCount;
        #endif
        for (const Event_ResourceReposition*e = begin; e!=end; ++e) {
            assert(e->_newResource && e->_originalResource && !e->_defragSteps.empty());

                // ... check temporary transactions ...
            for (unsigned c=0; c<temporaryCount; ++c) {
                Transaction& transaction = _transactions[c];
                if (transaction._finalResource->GetUnderlying().get() == e->_originalResource.get()) {
                    auto size = RenderCore::ByteCount(transaction._desc);

                    intrusive_ptr<ResourceLocator> oldLocator = std::move(transaction._finalResource);
                    unsigned oldOffset = oldLocator->Offset();
                    Resource_Validate(*oldLocator);

                    unsigned newOffsetValue = ResolveOffsetValue(oldOffset, RenderCore::ByteCount(transaction._desc), e->_defragSteps);
                    transaction._finalResource = make_intrusive<ResourceLocator>(
                        e->_newResource, newOffsetValue, unsigned(size), e->_pool, e->_poolMarker);
                }
            }

                // ... check long term transactions ...
            for (unsigned c=0; c<longTermCount; ++c) {
                #if defined(DEQUE_BASED_TRANSACTIONS)
                    Transaction& transaction = _transactions_LongTerm[c];
                #else
                    Transaction& transaction = _transactions[_transactions.size()-c-1];
                #endif
                if (transaction._finalResource->GetUnderlying().get() == e->_originalResource.get()) {
                    auto size = RenderCore::ByteCount(transaction._desc);

                    intrusive_ptr<ResourceLocator> oldLocator = std::move(transaction._finalResource);
                    unsigned oldOffset = oldLocator->Offset();
                    Resource_Validate(*oldLocator);

                    unsigned newOffsetValue = ResolveOffsetValue(oldOffset, RenderCore::ByteCount(transaction._desc), e->_defragSteps);
                    transaction._finalResource = make_intrusive<ResourceLocator>(
                        e->_newResource, newOffsetValue, unsigned(size), e->_pool, e->_poolMarker);
                }
            }
        }
        context.EventList_Release(id, true);
    }

    IManager::EventListID AssemblyLine::TickResourceSource(unsigned stepMask, ThreadContext& context, bool isLoading)
    {
        IManager::EventListID processedEventList     = context.EventList_GetProcessedID();
        IManager::EventListID publishableEventList   = context.EventList_GetWrittenID();

        if (stepMask & Step_DelayedReleases) {
            _resourceSource.FlushDelayedReleases(context.CommandList_GetCompletedByGPU());
        }

        if ((stepMask & Step_BatchedDefrag) && !isLoading) {        // don't do the defrag while we're loading

                //
                //      It's annoying, but we have to do the repositioning of the transactions list twice...
                //      Once to remove any new references to the old resource. And second to remove any 
                //      references that might have been added by the client through Transaction_Begin
                //
            if (_transactions_postPublishResolvedEventID < processedEventList) {
                for (unsigned c=_transactions_postPublishResolvedEventID+1; c<=processedEventList; ++c) { ApplyRepositionEvent(context, c); }
                _transactions_postPublishResolvedEventID = processedEventList;
            }

            static bool doDefrag = true;
            if (doDefrag) {
                bool deviceCreation = true;
                _resourceSource.Tick(context, processedEventList, deviceCreation);
                if (deviceCreation) {
                    CommandListMetrics& metricsUnderConstruction = context.GetMetricsUnderConstruction();
                    ++metricsUnderConstruction._countDeviceCreations[UploadDataType::Index];
                    ++metricsUnderConstruction._deviceCreateOperations;
                }
            }

            publishableEventList = context.EventList_GetWrittenID();

                //
                //      If we've got any completed/resolved reposition events, we need to modify any transactions in flight
                //      But -- don't lock the transactions list for this. Any newly added transactions from this pointer
                //          will be in the new coordinate system (because we're only resolving our references after the
                //          client has also done so)
                //

            if (_transactions_resolvedEventID < publishableEventList) {
                for (unsigned c=_transactions_resolvedEventID+1; c<=publishableEventList; ++c) { ApplyRepositionEvent(context, c); }
                _transactions_resolvedEventID = publishableEventList;
            }

                //
                //      Because we took EventList_GetProcessedID before we did FlushDelayedReleases(), all remaining releases in 
                //      the delayed releases list should be pointing to the new resource, and in the new coordinate system
                //
        }

        return publishableEventList;
    }

    AssemblyLine::BatchPreparation::BatchPreparation() { _batchedAllocationSize=0; }

    void AssemblyLine::ResolveBatchOperation(BatchPreparation& batchOperation, ThreadContext& context, unsigned stepMask)
    {
        CommandListMetrics& metricsUnderConstruction = context.GetMetricsUnderConstruction();
        if (!batchOperation._batchedSteps.empty() && batchOperation._batchedAllocationSize) {

                //
                //      Perform all batched operations before resolving a command list...
                //

            const unsigned maxSingleBatch = RenderCore::ByteCount(_resourceSource.GetBatchedResources().GetPrototype())/2;
            auto batchingI      = batchOperation._batchedSteps.begin();
            auto batchingStart  = batchOperation._batchedSteps.begin();
            unsigned currentBatchSize = 0;
            if (batchOperation._batchedAllocationSize <= maxSingleBatch) {
                batchingI = batchOperation._batchedSteps.end();
                currentBatchSize = batchOperation._batchedAllocationSize;
            }

                //
                //      Sort largest to smallest. This is an attempt to reduce fragmentation slightly by grouping
                //      large and small allocations. Plus, this should guarantee good packing into the batch size limit.
                //

            std::sort(batchOperation._batchedSteps.begin(), batchOperation._batchedSteps.end(), SortSize_SmallestToLargest);

            for (;;) {
                unsigned thisSize = 0;
                if (batchingI!=batchOperation._batchedSteps.end()) {
                    thisSize = MarkerHeap<uint16_t>::AlignSize(RenderCore::ByteCount(batchingI->_creationDesc));
                }
                if (batchingI == batchOperation._batchedSteps.end() || (currentBatchSize+thisSize) > maxSingleBatch) {
                    if (batchingI == batchingStart) {
                        ++batchingI;        // if a single object is larger than the batching size, we allocate it in one go
                    }

                    intrusive_ptr<ResourceLocator> batchedResource;
                    bool deviceAllocation = true;
                    for (;;) {
                        deviceAllocation = true;
                        batchedResource = _resourceSource.GetBatchedResources().Allocate(currentBatchSize, deviceAllocation, "SuperBlock");
                        if (batchedResource && !batchedResource->IsEmpty()) {
                            break;
                        }
                        Log(Warning) << "Resource creationg failed inBatchedResources::Allocate(). Sleeping and attempting again" << std::endl;
                        Threading::Sleep(16);
                    }

                    std::vector<unsigned> offsets;
                    offsets.resize(std::distance(batchingStart, batchingI), 0);

                        //
                        //      Success! Map & memcpy the data in. Use just one Map for all of the batched buffers, but
                        //      we can do separate memcpys.
                        //      We're using no-overwrite memcpys... so let's hope we get immediate access to the GPU buffer,
                        //      and can copy in the data (with a CPU-assisted copy, with no GPU cost associated)
                        //
                        //      We must do a discard map after a device creation on a D3D11 deferred context. But; there are some
                        //      cases were we should do a discard map on the resource even when deviceAllocation is false
                        //

                    if (stepMask & Step_BatchingUpload) {
                        const bool useMapPath = PlatformInterface::CanDoNooverwriteMapInBackground;  // the map path has some advantages; but has some problems in a background context... We could do it in a foreground context instead?
                        if (useMapPath) {

                            assert(0);
                            // PlatformInterface::UnderlyingDeviceContext::MapType::Enum mapType = PlatformInterface::UnderlyingDeviceContext::MapType::NoOverwrite;
                            // PlatformInterface::UnderlyingDeviceContext::MappedBuffer mappedBuffer = context.GetDeviceContext().Map(*batchedResource->GetUnderlying(), mapType);
                            // CopyIntoBatchedBuffer(mappedBuffer.GetData(), AsPointer(batchingStart), AsPointer(batchingI), batchedResource->GetUnderlying(), batchedResource->Offset(), AsPointer(offsets.begin()), metricsUnderConstruction);

                        } else {

                            BasicRawDataPacket midwayBuffer(currentBatchSize);
                            CopyIntoBatchedBuffer(
                                PtrAdd(midwayBuffer.GetData(),-ptrdiff_t(batchedResource->Offset())), 
                                AsPointer(batchingStart), AsPointer(batchingI), 
                                batchedResource->GetUnderlying().get(), batchedResource->Offset(), 
                                AsPointer(offsets.begin()), metricsUnderConstruction);

                            assert(_resourceSource.GetBatchedResources().GetPrototype()._type == ResourceDesc::Type::LinearBuffer);
                            context.GetDeviceContext().WriteToBufferViaMap(
                                *batchedResource->GetUnderlying(), _resourceSource.GetBatchedResources().GetPrototype(), 
                                batchedResource->Offset(), midwayBuffer.GetData(), currentBatchSize);

                        }
                    } else {

                            //
                            //      This will offload the actual map & copy into another thread. This seems to be a little better for D3D when
                            //      we want to write directly into video memory. Note that there's a copy step here, though -- so we don't get 
                            //      the minimum number of copies
                            //

                        auto midwayBuffer = make_intrusive<BasicRawDataPacket>(currentBatchSize);
                        CopyIntoBatchedBuffer(
                            PtrAdd(midwayBuffer->GetData(),-ptrdiff_t(batchedResource->Offset())), 
                            AsPointer(batchingStart), AsPointer(batchingI), 
                            batchedResource->GetUnderlying().get(), batchedResource->Offset(), 
                            AsPointer(offsets.begin()), metricsUnderConstruction);
                        context.GetCommitStepUnderConstruction().Add(
                            CommitStep::DeferredCopy(batchedResource, currentBatchSize, std::move(midwayBuffer)));

                    }

                    metricsUnderConstruction._batchedCopyBytes += currentBatchSize;
                    metricsUnderConstruction._bytesUploadTotal += currentBatchSize;
                    metricsUnderConstruction._batchedCopyCount ++;

                        // now apply the result to the transactions, and release them...
                    auto o=offsets.begin();
                    for (auto i=batchingStart; i!=batchingI; ++i, ++o) {
                        Transaction* transaction = GetTransaction(i->_id);
                        transaction->_finalResource = make_intrusive<ResourceLocator>(
                            batchedResource->GetUnderlying(), *o, RenderCore::ByteCount(i->_creationDesc),
                            batchedResource->Pool(), batchedResource->PoolMarker());
                        ReleaseTransaction(transaction, context);
                    }

                    if (deviceAllocation) {
                        ++metricsUnderConstruction._countDeviceCreations[
                            (unsigned)AsUploadDataType(_resourceSource.GetBatchedResources().GetPrototype())];
                    }

                    if (batchingI == batchOperation._batchedSteps.end()) {
                        break;
                    }
                    batchingStart = batchingI;
                    currentBatchSize = 0;
                } else {
                    ++batchingI;
                    currentBatchSize+=thisSize;
                }
            }
        }
    }

#else

    IManager::EventListID AssemblyLine::TickResourceSource(unsigned stepMask, ThreadContext& context, bool isLoading)
    {
        IManager::EventListID processedEventList     = context.EventList_GetProcessedID();
        IManager::EventListID publishableEventList   = context.EventList_GetWrittenID();

        if (stepMask & Step_DelayedReleases) {
            _resourceSource.FlushDelayedReleases(context.CommandList_GetCompletedByGPU());
        }

        return publishableEventList;
    }

#endif

    AssemblyLine::CommandListBudget::CommandListBudget(bool isLoading)
    {
        if (true) { // isLoading) {
            _limit_BytesUploaded     = ~unsigned(0x0);
            _limit_Operations        = ~unsigned(0x0);
            _limit_DeviceCreates     = ~unsigned(0x0);
        } else {
                // ~    Default budget during run-time    ~ //
            _limit_BytesUploaded     = 5 * 1024 * 1024;
            _limit_Operations        = 64;
            _limit_DeviceCreates     = 32;
        }
    }

    void AssemblyLine::OnLostDevice()
    {
            //
            //      On Lost Device, we must go through and destroy all of the resources currently
            //      sitting in the transactions buffer.
            //
            //      But this will mean that some operations get canceled as a result... If the 
            //      all of the create/upload/etc operations have been completed, then they won't
            //      be run again. It's best if the client cancels the operation on their side, as well.
            //
            //      Even worse... If a create operation has been completed, but the corresponding
            //      upload operation hasn't, then that upload can lock up waiting for a complete
            //      that might never come.
            //
            //      So the client must cancel all pending operations as well!
            //

        for (unsigned c=0; c<_transactions.size(); ++c) {
            if (!(_transactions[c]._desc._allocationRules & BufferUploads::AllocationRules::NonVolatile)) {
                _transactions[c]._finalResource = {};
                // _transactions[c]._stagingResource = {};
            }
        }

        #if defined(DEQUE_BASED_TRANSACTIONS)
            for (unsigned c=0; c<_transactions_LongTerm.size(); ++c) {
                if (!(_transactions_LongTerm[c]._desc._allocationRules & BufferUploads::AllocationRules::NonVolatile)) {
                    _transactions_LongTerm[c]._finalResource = {};
                    // _transactions_LongTerm[c]._stagingResource = {};
                }
            }
        #endif

        _resourceSource.OnLostDevice();
#if BU_BATCHING
        _batchPreparation_Main = BatchPreparation();        // cancel whatever was happening here
#endif
    }

    bool AssemblyLine::Process(const CreateFromDataPacketStep& resourceCreateStep, ThreadContext& context, const CommandListBudget& budgetUnderConstruction)
    {
        auto& metricsUnderConstruction = context.GetMetricsUnderConstruction();
        if ((metricsUnderConstruction._contextOperations+1) >= budgetUnderConstruction._limit_Operations)
            return false;

        auto* transaction = GetTransaction(resourceCreateStep._id);
        assert(transaction && !transaction->_finalResource._resource);

        unsigned uploadRequestSize = 0;
        const unsigned objectSize = RenderCore::ByteCount(transaction->_desc);
        auto uploadDataType = (unsigned)AsUploadDataType(transaction->_desc);
        if (resourceCreateStep._initialisationData) {
            uploadRequestSize = objectSize;
        }
        
        if (!(transaction->_referenceCount & 0xff000000)) {
                //  If there are no client references, we can consider this cancelled...
            ReleaseTransaction(transaction, context, true);
            _currentQueuedBytes[uploadDataType] -= uploadRequestSize;
            return true;
        }

        if ((metricsUnderConstruction._bytesUploadTotal+uploadRequestSize) > budgetUnderConstruction._limit_BytesUploaded && metricsUnderConstruction._bytesUploadTotal !=0)
            return false;

        auto finalConstruction = _resourceSource.Create(
            transaction->_desc, resourceCreateStep._initialisationData.get(), 
            ((metricsUnderConstruction._deviceCreateOperations+1) <= budgetUnderConstruction._limit_DeviceCreates)?ResourceSource::CreationOptions::AllowDeviceCreation:0);

        if (finalConstruction._flags & ResourceSource::ResourceConstruction::Flags::DelayForBatching) {
                //      In the batched path, we pop now, and perform all of the batched operations as once when we resolve the 
                //      command list. But don't release the transaction -- that will happen after the batching operation is 
                //      performed.
            #if BU_BATCHING
                _batchPreparation_Main._batchedSteps.push_back(resourceCreateStep);
                _batchPreparation_Main._batchedAllocationSize += MarkerHeap<uint16_t>::AlignSize(objectSize);
            #else  
                assert(0);
            #endif
            return true;
        }

        if (!finalConstruction._locator._resource)
            return false;

        if (resourceCreateStep._initialisationData && !(finalConstruction._flags & ResourceSource::ResourceConstruction::Flags::InitialisationSuccessful)) {
            ResourceDesc stagingDesc;
            PlatformInterface::StagingToFinalMapping stagingToFinalMapping;
            std::tie(stagingDesc, stagingToFinalMapping) = CalculatePartialStagingDesc(transaction->_desc, resourceCreateStep._part);

            auto stagingConstruction = _resourceSource.Create(
                stagingDesc, resourceCreateStep._initialisationData.get(), ResourceSource::CreationOptions::AllowDeviceCreation);
            assert(stagingConstruction._locator._resource);
            if (!stagingConstruction._locator._resource)
                return false;
    
            PlatformInterface::UnderlyingDeviceContext deviceContext(context.GetDeviceContext());
            deviceContext.WriteToTextureViaMap(
                *stagingConstruction._locator._resource,
                stagingDesc, Box2D(),
                [part{resourceCreateStep._part}, initialisationData{resourceCreateStep._initialisationData.get()}](RenderCore::SubResourceId sr) -> RenderCore::SubResourceInitData
                {
                    RenderCore::SubResourceInitData result = {};
                    auto size = initialisationData->GetDataSize(SubResourceId{sr._mip, sr._arrayLayer});
                    const void* data = initialisationData->GetData(SubResourceId{sr._mip, sr._arrayLayer});
                    assert(data);
                    result._data = MakeIteratorRange(data, PtrAdd(data, size));
                    result._pitches = initialisationData->GetPitches(SubResourceId{sr._mip, sr._arrayLayer});
                    return result;
                });
    
            deviceContext.UpdateFinalResourceFromStaging(
                *finalConstruction._locator._resource, 
                *stagingConstruction._locator._resource, transaction->_desc, 
                stagingToFinalMapping);
                
            ++metricsUnderConstruction._contextOperations;
            metricsUnderConstruction._stagingBytesUsed[uploadDataType] += uploadRequestSize;
        }

        metricsUnderConstruction._bytesUploaded[uploadDataType] += uploadRequestSize;
        metricsUnderConstruction._bytesUploadTotal += uploadRequestSize;
        _currentQueuedBytes[uploadDataType] -= uploadRequestSize;
        metricsUnderConstruction._bytesCreated[uploadDataType] += objectSize;
        metricsUnderConstruction._countCreations[uploadDataType] += 1;
        if (finalConstruction._flags & ResourceSource::ResourceConstruction::Flags::DeviceConstructionInvoked) {
            ++metricsUnderConstruction._countDeviceCreations[uploadDataType];
            ++metricsUnderConstruction._deviceCreateOperations;
        }

        transaction->_finalResource = std::move(finalConstruction._locator);
        transaction->_promise.set_value(transaction->_finalResource);

        ReleaseTransaction(transaction, context);
        return true;
    }

    bool AssemblyLine::Process(const PrepareStagingStep& prepareStagingStep, ThreadContext& context, const CommandListBudget& budgetUnderConstruction)
    {
        auto& metricsUnderConstruction = context.GetMetricsUnderConstruction();
        if ((metricsUnderConstruction._contextOperations+1) >= budgetUnderConstruction._limit_Operations)
            return false;

        // todo -- should we limit this based on the number of items in the WaitForDataFutureStep
        //      stage?

        auto* transaction = GetTransaction(prepareStagingStep._id);
        assert(transaction);

        if (!(transaction->_referenceCount & 0xff000000)) {
            ReleaseTransaction(transaction, context, true);
            return true;
        }

        try {
            const auto& desc = prepareStagingStep._desc;
            ResourceDesc stagingDesc;
            PlatformInterface::StagingToFinalMapping stagingToFinalMapping;
            std::tie(stagingDesc, stagingToFinalMapping) = CalculatePartialStagingDesc(desc, prepareStagingStep._part);

            auto stagingConstruction = _resourceSource.Create(
                stagingDesc, nullptr, ResourceSource::CreationOptions::AllowDeviceCreation);
            assert(stagingConstruction._locator._resource);
            if (!stagingConstruction._locator._resource)
                return false;

            using namespace RenderCore;

            auto dstLodLevelMax = std::min(stagingToFinalMapping._dstLodLevelMax, (unsigned)desc._textureDesc._mipCount-1);
            auto dstArrayLayerMax = std::min(stagingToFinalMapping._dstArrayLayerMax, (unsigned)desc._textureDesc._arrayCount-1);
            auto mipCount = dstLodLevelMax - stagingToFinalMapping._dstLodLevelMin + 1;
            auto arrayCount = dstArrayLayerMax - stagingToFinalMapping._dstArrayLayerMin + 1;
            if (desc._textureDesc._arrayCount == 0) {
                dstArrayLayerMax = 0;
                arrayCount = 1;
            }
            assert(mipCount >= 1);
            assert(arrayCount >= 1);

            IAsyncDataSource::SubResource uploadList[mipCount*arrayCount];
            std::vector<Metal::ResourceMap> maps;
            maps.resize(mipCount*arrayCount);

            for (unsigned a=stagingToFinalMapping._dstArrayLayerMin; a<=dstArrayLayerMax; ++a) {
                for (unsigned mip=stagingToFinalMapping._dstLodLevelMin; mip<=dstLodLevelMax; ++mip) {
                    SubResourceId subRes { mip - stagingToFinalMapping._stagingLODOffset, a - stagingToFinalMapping._stagingArrayOffset };
                    auto idx = subRes._arrayLayer*mipCount+subRes._mip;
                    
                    maps[idx] = Metal::ResourceMap(
                        *Metal::DeviceContext::Get(context.GetDeviceContext().GetUnderlying()),
                        *stagingConstruction._locator._resource,
                        Metal::ResourceMap::Mode::WriteDiscardPrevious,
                        subRes);

                    auto& upload = uploadList[idx];
                    upload._id = subRes;
                    upload._destination = maps[idx].GetData();
                    upload._pitches = maps[idx].GetPitches();
                }
            }

            auto future = prepareStagingStep._packet->PrepareData(MakeIteratorRange(uploadList, &uploadList[mipCount*arrayCount]));

            transaction->_desc = desc;
            auto byteCount = RenderCore::ByteCount(stagingDesc);
            _currentQueuedBytes[(unsigned)AsUploadDataType(desc)] += byteCount;
            metricsUnderConstruction._stagingBytesUsed[(unsigned)AsUploadDataType(desc)] += byteCount;

            // inc reference count for the lambda that waits on the future
            ++transaction->_referenceCount;

            auto weakThis = weak_from_this();
            assert(!transaction->_waitingFuture.valid());
            transaction->_waitingFuture = thousandeyes::futures::then(
                std::move(future),
                [   captureMaps{std::move(maps)}, 
                    weakThis, 
                    transactionID{prepareStagingStep._id}, 
                    locator{stagingConstruction._locator},
                    stagingToFinalMapping, byteCount]
                (std::future<void> prepareFuture) mutable {
                    captureMaps.clear();

                    auto t = weakThis.lock();
                    if (!t)
                        Throw(std::runtime_error("Assembly line was destroyed before future completed"));

                    t->CompleteWaitForDataFuture(transactionID, std::move(prepareFuture), locator, stagingToFinalMapping, byteCount);
                });

        } catch (...) {
            transaction->_promise.set_exception(std::current_exception());
        }

        ReleaseTransaction(transaction, context, true);
        return true;
    }

    void    AssemblyLine::CompleteWaitForDescFuture(TransactionID transactionID, std::future<ResourceDesc> descFuture, const std::shared_ptr<IAsyncDataSource>& data, PartialResource part)
    {
        Transaction* transaction = GetTransaction(transactionID);
        assert(transaction);

        transaction->_waitingFuture = {};
        
        try {
            auto desc = descFuture.get();
            _currentQueuedBytes[(unsigned)AsUploadDataType(desc)] += RenderCore::ByteCount(desc);
            PushStep(
                GetQueueSet(transaction->_creationOptions),
                *transaction,
                PrepareStagingStep { transactionID, desc, data, PartialResource_All() });
        } catch (...) {
            transaction->_promise.set_exception(std::current_exception());
        }

        _queuedFunctions.push(
            [transactionID](AssemblyLine& assemblyLine, ThreadContext& context) {
                Transaction* transaction = assemblyLine.GetTransaction(transactionID);
                assert(transaction);
                assemblyLine.ReleaseTransaction(transaction, context, true);
            });
        _wakeupEvent.Increment();
    }

    void AssemblyLine::CompleteWaitForDataFuture(TransactionID transactionID, std::future<void> prepareFuture, const ResourceLocator& stagingResource, const PlatformInterface::StagingToFinalMapping& stagingToFinalMapping, unsigned stagingByteCount)
    {
        auto* transaction = GetTransaction(transactionID);
        assert(transaction);

        transaction->_waitingFuture = {};

        // Any exceptions get passed along to the transaction's future. Otherwise we just queue up the
        // next step
        try {
            prepareFuture.get();
            PushStep(
                GetQueueSet(transaction->_creationOptions),
                *transaction,
                TransferStagingToFinalStep { transactionID, stagingResource, stagingToFinalMapping, stagingByteCount });
        } catch(...) {
            transaction->_promise.set_exception(std::current_exception());
        }

        _queuedFunctions.push(
            [transactionID](AssemblyLine& assemblyLine, ThreadContext& context) {
                Transaction* transaction = assemblyLine.GetTransaction(transactionID);
                assert(transaction);
                assemblyLine.ReleaseTransaction(transaction, context, true);
            });
        _wakeupEvent.Increment();
    }

    bool AssemblyLine::Process(const TransferStagingToFinalStep& transferStagingToFinalStep, ThreadContext& context, const CommandListBudget& budgetUnderConstruction)
    {
        CommandListMetrics& metricsUnderConstruction = context.GetMetricsUnderConstruction();
        if ((metricsUnderConstruction._contextOperations+1) >= budgetUnderConstruction._limit_Operations)
            return false;

        Transaction* transaction = GetTransaction(transferStagingToFinalStep._id);
        assert(transaction);
        auto dataType = (unsigned)AsUploadDataType(transaction->_desc);

        if (!(transaction->_referenceCount & 0xff000000)) {
            ReleaseTransaction(transaction, context, true);
            _currentQueuedBytes[dataType] -= transferStagingToFinalStep._stagingByteCount;
            return true;
        }

        if ((metricsUnderConstruction._bytesUploadTotal+transferStagingToFinalStep._stagingByteCount) > budgetUnderConstruction._limit_BytesUploaded && metricsUnderConstruction._bytesUploadTotal !=0)
            return false;

        if (!transaction->_finalResource._resource) {
            auto finalConstruction = _resourceSource.Create(
                transaction->_desc, nullptr, ResourceSource::CreationOptions::AllowDeviceCreation);
            if (!finalConstruction._locator._resource)
                return false;                   // failed to allocate the resource. Return false and We'll try again later...

            transaction->_finalResource = finalConstruction._locator;

            metricsUnderConstruction._bytesCreated[dataType] += RenderCore::ByteCount(transaction->_desc);
            metricsUnderConstruction._countCreations[dataType] += 1;
            metricsUnderConstruction._countDeviceCreations[dataType] += (finalConstruction._flags&ResourceSource::ResourceConstruction::Flags::DeviceConstructionInvoked)?1:0;
        }

        // Do the actual data copy step here
        PlatformInterface::UnderlyingDeviceContext deviceContext(context.GetDeviceContext());
        deviceContext.UpdateFinalResourceFromStaging(
            *transaction->_finalResource._resource, 
            *transferStagingToFinalStep._stagingResource._resource, transaction->_desc, 
            transferStagingToFinalStep._stagingToFinalMapping);

        metricsUnderConstruction._bytesUploadTotal += transferStagingToFinalStep._stagingByteCount;
        metricsUnderConstruction._bytesUploaded[dataType] += transferStagingToFinalStep._stagingByteCount;
        metricsUnderConstruction._countUploaded[dataType] += 1;
        _currentQueuedBytes[dataType] -= transferStagingToFinalStep._stagingByteCount;
        ++metricsUnderConstruction._contextOperations;
        transaction->_promise.set_value(transaction->_finalResource);

        ReleaseTransaction(transaction, context);
        return true;
    }

#if 0
    bool AssemblyLine::Process(const DataUploadStep& uploadStep, unsigned stepMask, ThreadContext& context, const CommandListBudget& budgetUnderConstruction)
    {
        CommandListMetrics& metricsUnderConstruction = context.GetMetricsUnderConstruction();
        if ((metricsUnderConstruction._contextOperations+metricsUnderConstruction._nonContextOperations+1) < budgetUnderConstruction._limit_Operations) {
            Transaction* transaction = GetTransaction(uploadStep._id);
            assert(transaction);

            unsigned uploadRequestSize = 0;
            for (unsigned l=uploadStep._lodLevelMin; l<=uploadStep._lodLevelMax; ++l) {
                uploadRequestSize += (unsigned)uploadStep._rawData->GetDataSize(SubR(l, uploadStep._arrayIndex));
            }
                
            if (!(transaction->_referenceCount & 0xff000000) && (!transaction->_finalResource.get() || transaction->_finalResource->IsEmpty())) {
                ReleaseTransaction(transaction, context, true);
                _currentQueuedBytes[(unsigned)AsUploadDataType(transaction->_desc)] -= uploadRequestSize;
                return true;
            }

            const bool readyToUpload = transaction->_finalResource && !transaction->_finalResource->IsEmpty()
                && ((transaction->_stagingResource&&uploadStep._lodLevelMin>=transaction->_actualisedStagingLODOffset)||!transaction->_stagingQueued);
            if (readyToUpload) {

                if ((metricsUnderConstruction._bytesUploadTotal+uploadRequestSize) <= budgetUnderConstruction._limit_BytesUploaded || !metricsUnderConstruction._bytesUploadTotal) {

                        //
                        //      If we're writing to a batching buffer, there must be a special path. This is because
                        //      we're writing to only a part of the entire buffer -- and we can't necessarily do a UpdateSubresource
                        //      to modify only part.
                        //
                            
                    unsigned bytesUploaded = 0, uploadCount = 0;
                    bool doDeferredBatchingLoad = false;
                    auto locator = transaction->_finalResource;
                    if (!(stepMask & Step_BatchingUpload)) {
                        doDeferredBatchingLoad = !!_resourceSource.IsBatchedResource(*locator, transaction->_desc);
                        assert(_resourceSource.IsBatchedResource(*locator, transaction->_desc) != BatchedResources::ResultFlags::IsCurrentlyDefragging);
                    }
                    if (!doDeferredBatchingLoad) {
                        if (transaction->_stagingQueued) {          //~~//////////////////////~~//

                                //
                                //      Push via the staging result. Empty the raw data pointer
                                //      into the staging object, and then submit to the final result.
                                //

                            Box2D stagingBox;
                            const bool stageInTopLeftCorner = false;
                            if (constant_expression<stageInTopLeftCorner>::result()) {
                                stagingBox = Box2D{
                                    0, 0, 
                                    uploadStep._destinationBox._right - uploadStep._destinationBox._left,
                                    uploadStep._destinationBox._bottom - uploadStep._destinationBox._top};
                            } else {
                                stagingBox = uploadStep._destinationBox;
                            }

                            auto finalDesc = ApplyLODOffset(transaction->_desc, transaction->_actualisedStagingLODOffset);
                            auto mipOffset = transaction->_actualisedStagingLODOffset;
                            bytesUploaded += context.GetDeviceContext().WriteToTextureViaMap(
                                *transaction->_stagingResource->GetUnderlying(), 
                                finalDesc, stagingBox,
                                [&uploadStep, mipOffset](RenderCore::SubResourceId sr) -> RenderCore::SubResourceInitData
                                {
                                    RenderCore::SubResourceInitData result = {};
                                    if (sr._arrayLayer != uploadStep._arrayIndex) return result;
                                    if (sr._mip < uploadStep._lodLevelMin || sr._mip > uploadStep._lodLevelMax) return result;

                                    auto dataMip = sr._mip + mipOffset;
                                    auto size = uploadStep._rawData->GetDataSize(SubR(dataMip, sr._arrayLayer));
                                    const void* data = uploadStep._rawData->GetData(SubR(dataMip, sr._arrayLayer));
									result._data = MakeIteratorRange(data, PtrAdd(data, size));
                                    result._pitches = uploadStep._rawData->GetPitches(SubR(dataMip, sr._arrayLayer));
                                    return result;
                                });
                            ++uploadCount;

                            assert(transaction->_finalResource->Offset()==0||transaction->_finalResource->Offset()==~unsigned(0x0));       // resource offsets not correctly implemented
                            context.GetDeviceContext().UpdateFinalResourceFromStaging(
                                *transaction->_finalResource->GetUnderlying(), *transaction->_stagingResource->GetUnderlying(), transaction->_desc, 
                                uploadStep._lodLevelMin, uploadStep._lodLevelMax, transaction->_actualisedStagingLODOffset,
                                {(unsigned)uploadStep._destinationBox._left, (unsigned)uploadStep._destinationBox._top},
                                stagingBox);

                        } else {                                    //~~//////////////////////~~//

                                //
                                //      Update directly to the resource, without going through the staging resource.
                                //      This is the only way that works with when building resources in a background
                                //      thread in D3D11 (since we can't lock and fill in a staging resource).
                                //

                            ResourceDesc stagingDesc = transaction->_desc;
                            stagingDesc._cpuAccess = CPUAccess::Read|CPUAccess::Write;  // the CPUAccess::WriteDynamic flag was being sent to PushToResource(), which confused the logic below

                            if (stagingDesc._type == ResourceDesc::Type::LinearBuffer) {
                                bytesUploaded += context.GetDeviceContext().WriteToBufferViaMap(
                                    *transaction->_finalResource->GetUnderlying(), stagingDesc, transaction->_finalResource->Offset(),
                                    uploadStep._rawData->GetData(), uploadStep._rawData->GetDataSize());
                            } else {
                                bytesUploaded += context.GetDeviceContext().WriteToTextureViaMap(
                                    *transaction->_finalResource->GetUnderlying(), stagingDesc, 
                                    uploadStep._destinationBox,
                                    [&uploadStep](RenderCore::SubResourceId sr) -> RenderCore::SubResourceInitData
                                    {
                                        RenderCore::SubResourceInitData result = {};
                                        if (sr._arrayLayer != uploadStep._arrayIndex) return result;
                                        if (sr._mip < uploadStep._lodLevelMin || sr._mip > uploadStep._lodLevelMax) return result;

                                        auto dataMip = sr._mip;
                                        auto size = uploadStep._rawData->GetDataSize(SubR(dataMip, sr._arrayLayer));
                                        const void* data = uploadStep._rawData->GetData(SubR(dataMip, sr._arrayLayer));
										result._data = MakeIteratorRange(data, PtrAdd(data, size));
                                        result._pitches = uploadStep._rawData->GetPitches(SubR(dataMip, sr._arrayLayer));
                                        return result;
                                    });
                            }
                            ++uploadCount;
                        }                                           //~~//////////////////////~~//
                    } else {
                        assert(uploadStep._lodLevelMin == 0 && uploadStep._lodLevelMax == 0 && uploadStep._arrayIndex <= 1);
                        bytesUploaded = (unsigned)uploadStep._rawData->GetDataSize();
                        context.GetCommitStepUnderConstruction().Add(
                            CommitStep::DeferredCopy(transaction->_finalResource, bytesUploaded, uploadStep._rawData));
                    }

                    auto dataType = AsUploadDataType(transaction->_desc);
                    metricsUnderConstruction._bytesUploaded[(unsigned)dataType] += bytesUploaded;
                    metricsUnderConstruction._countUploaded[(unsigned)dataType] += uploadCount;
                    metricsUnderConstruction._bytesUploadTotal += bytesUploaded;
                    _currentQueuedBytes[(unsigned)dataType] -= bytesUploaded;
                    ++metricsUnderConstruction._contextOperations;

                    assert(_resourceSource.IsBatchedResource(*locator, transaction->_desc) != BatchedResources::ResultFlags::IsCurrentlyDefragging);

                        //
                        //      Pop our "step" -- there's no more to do here
                        //

                    ReleaseTransaction(transaction, context);
                    return true;
                }
            }
        }
        return false;
    }

    bool        AssemblyLine::Process(
        QueueSet& queueSet,
        const PrepareDataStep& step, unsigned stepMask, ThreadContext& context, 
        const CommandListBudget& budgetUnderConstruction, bool stallOnPending)
    {
        CommandListMetrics& metricsUnderConstruction = context.GetMetricsUnderConstruction();
        if ((metricsUnderConstruction._contextOperations+metricsUnderConstruction._nonContextOperations+1) < budgetUnderConstruction._limit_Operations) {
            Transaction* transaction = GetTransaction(step._id);
            assert(transaction);

            if (!(transaction->_referenceCount & 0xff000000) || !step._marker) {
                    // Cancelling because the client dropped the last transaction reference
                    // We should ideally also cancel the bkground operation here... If we don't
                    // cancel it, it should complete as normally, but the result will go unused
                ReleaseTransaction(transaction, context, true);
                return true;
            }

            auto currentState = step._marker->GetAssetState();
            if (currentState == Assets::AssetState::Pending) {
                if (!stallOnPending)
                    return false; // still waiting

                auto res = step._marker->StallWhilePending();
				if (!res.has_value()) {
					ReleaseTransaction(transaction, context, true);
					return false;
				}
				currentState = res.value();
            }

            if (currentState == Assets::AssetState::Ready) {
                // Asset is ready. We should now have a full buffer desc for this
                // object. That means we can create the resource creation and 
                // upload operations.

                auto part = step._part;
				auto desc = step._packet->GetDesc();
                if (desc._type == ResourceDesc::Type::Texture) {
                    transaction->_desc._type = desc._type;
                    transaction->_desc._textureDesc = desc._textureDesc;

                    part = DefaultPartialResource(transaction->_desc, *step._packet);
                } else if (desc._type == ResourceDesc::Type::LinearBuffer) {
                    transaction->_desc._type = desc._type;
                    transaction->_desc._linearBufferDesc = desc._linearBufferDesc;
                } // else if step._marker->_desc._type == ResourceDesc::Type::Unknown, do nothing
                UpdateData_PostBackground(queueSet, *transaction, step._id, step._packet.get(), part);
            } else {
                assert(currentState == Assets::AssetState::Invalid);

                // Asset is invalid (eg, a missing file)
                // this should complete the transaction -- but leave it in an invalid state
            }

            ReleaseTransaction(transaction, context, true);
            return true;
        }

        return false;
    }
#endif

    bool        AssemblyLine::DrainPriorityQueueSet(QueueSet& queueSet, unsigned stepMask, ThreadContext& context)
    {
        bool didSomething = false;
        CommandListBudget budgetUnderConstruction(true);

            /////////////// ~~~~ /////////////// ~~~~ ///////////////
        if (stepMask & Step_PrepareStaging) {
            while (!queueSet._prepareStagingSteps.empty()) {
                if (Process(queueSet._prepareStagingSteps.front(), context, budgetUnderConstruction)) {
                    didSomething = true;
                } else {
                    _queueSet_Main._prepareStagingSteps.push(std::move(queueSet._prepareStagingSteps.front()));
                }
                queueSet._prepareStagingSteps.pop();
            }
        }

            /////////////// ~~~~ /////////////// ~~~~ ///////////////
        if (stepMask & Step_TransferStagingToFinal) {
            TransferStagingToFinalStep* step = 0;
            while (!queueSet._transferStagingToFinalSteps.empty()) {
                if (Process(queueSet._transferStagingToFinalSteps.front(), context, budgetUnderConstruction)) {
                    didSomething = true;
                } else {
                    _queueSet_Main._transferStagingToFinalSteps.push(std::move(queueSet._transferStagingToFinalSteps.front()));
                }
                queueSet._transferStagingToFinalSteps.pop();
            }
        }

            /////////////// ~~~~ /////////////// ~~~~ ///////////////
        if (stepMask & Step_CreateFromDataPacket) {
            CreateFromDataPacketStep* step = 0;
            while (!queueSet._createFromDataPacketSteps.empty()) {
                if (Process(queueSet._createFromDataPacketSteps.front(), context, budgetUnderConstruction)) {
                    didSomething = true;
                } else {
                    _queueSet_Main._createFromDataPacketSteps.push(std::move(queueSet._createFromDataPacketSteps.front()));
                }
                queueSet._createFromDataPacketSteps.pop();
            }
        }

        return didSomething;
    }

    std::pair<bool,bool> AssemblyLine::ProcessQueueSet(QueueSet& queueSet, unsigned stepMask, ThreadContext& context, const CommandListBudget& budgetUnderConstruction)
    {
        bool nothingFoundInQueues = true;
        bool atLeastOneRealAction = false;

            /////////////// ~~~~ /////////////// ~~~~ ///////////////
        if (stepMask & Step_PrepareStaging) {
            PrepareStagingStep* step = 0;
            if (!queueSet._prepareStagingSteps.empty()) {
                if (Process(queueSet._prepareStagingSteps.front(), context, budgetUnderConstruction)) {
                    atLeastOneRealAction = true;
                    queueSet._prepareStagingSteps.pop();
                }
                nothingFoundInQueues = false;
            }
        }

            /////////////// ~~~~ /////////////// ~~~~ ///////////////
        if (stepMask & Step_TransferStagingToFinal) {
            TransferStagingToFinalStep* step = 0;
            if (!queueSet._transferStagingToFinalSteps.empty()) {
                if (Process(queueSet._transferStagingToFinalSteps.front(), context, budgetUnderConstruction)) {
                    atLeastOneRealAction = true;
                    queueSet._transferStagingToFinalSteps.pop();
                }
                nothingFoundInQueues = false;
            }
        }

            /////////////// ~~~~ /////////////// ~~~~ ///////////////
        if (stepMask & Step_CreateFromDataPacket) {
            CreateFromDataPacketStep* step = 0;
            if (!queueSet._createFromDataPacketSteps.empty()) {
                if (Process(queueSet._createFromDataPacketSteps.front(), context, budgetUnderConstruction)) {
                    atLeastOneRealAction = true;
                    queueSet._createFromDataPacketSteps.pop();
                }
                nothingFoundInQueues = false;
            }
        }

        return std::make_pair(nothingFoundInQueues, atLeastOneRealAction);
    }

    void AssemblyLine::Process(unsigned stepMask, ThreadContext& context)
    {
        const bool          isLoading = false;
        CommandListMetrics& metricsUnderConstruction = context.GetMetricsUnderConstruction();
        CommandListBudget   budgetUnderConstruction(isLoading);

        _queuedWorkFlag = true;
        for (;;) {
            bool nothingFoundInQueues = true, atLeastOneRealAction = false;

                /////////////// ~~~~ /////////////// ~~~~ ///////////////
            IManager::EventListID publishableEventList = TickResourceSource(stepMask, context, isLoading);

            while (!_queuedFunctions.empty()) {
                _queuedFunctions.front().operator()(*this, context);
                _queuedFunctions.pop();
            }

            bool framePriorityResolve = false;
            unsigned *qs = NULL;

            unsigned fromFramePriorityQueueSet = ~unsigned(0x0);

            if (context._pendingFramePriority_CommandLists.try_front(qs)) {

                    //      --~<   Drain all frame priority steps   >~--      //
                if (DrainPriorityQueueSet(_queueSet_FramePriority[*qs], stepMask, context)) {
                    nothingFoundInQueues = false;
                    atLeastOneRealAction = true;
                }
                framePriorityResolve = true;
                fromFramePriorityQueueSet = *qs;

            } else {

                    //
                    //      Process the queue set, but do everything in the "frame priority" queue set that we're writing 
                    //      to first. This may sometimes do things out of order, but it means the higher priority
                    //      things will complete first
                    //

                std::pair<bool,bool> t2 = ProcessQueueSet(_queueSet_FramePriority[_framePriority_WritingQueueSet], stepMask, context, budgetUnderConstruction);
                nothingFoundInQueues  &= t2.first;
                atLeastOneRealAction  |= t2.second;
                if (atLeastOneRealAction) {
                    fromFramePriorityQueueSet = _framePriority_WritingQueueSet;
                }

                if (nothingFoundInQueues) {
                    std::pair<bool,bool> t = ProcessQueueSet(_queueSet_Main, stepMask, context, budgetUnderConstruction);
                    nothingFoundInQueues  &= t.first;
                    atLeastOneRealAction  |= t.second;
                }

                //      this can happen when we're pending a file load now
                // if (!nothingFoundInQueues && !atLeastOneRealAction) {
                //     LogWarning << "Suspected allocation failure; sleeping";
                //     Sleep(5);
                // }
            }

            CommandList::ID commandListIdCommitted = ~unsigned(0x0);

                /////////////// ~~~~ /////////////// ~~~~ ///////////////
            const bool somethingToResolve = 
                    (metricsUnderConstruction._contextOperations!=0)
#if BU_BATCHING
                ||  _batchPreparation_Main._batchedAllocationSize  || !context.GetCommitStepUnderConstruction().IsEmpty()
#endif
                ||  publishableEventList > context.EventList_GetPublishedID();
            const unsigned commitCountCurrent = context.CommitCount_Current();
            const bool normalPriorityResolve = commitCountCurrent > context.CommitCount_LastResolve();
            if ((framePriorityResolve||normalPriorityResolve) && somethingToResolve) {
                commandListIdCommitted = context.CommandList_GetUnderConstruction();

                // OutputDebugString(FormatString("Resolving command list: (%i)\n", commandListIdCommitted).c_str());
                // OutputDebugString(FormatString("   Context operations: (%i)\n", metricsUnderConstruction._contextOperations).c_str());
                // OutputDebugString(FormatString("   Non context operations: (%i)\n", metricsUnderConstruction._nonContextOperations).c_str());
                // OutputDebugString(FormatString("   Batched Allocation Size: (%i)\n", _batchPreparation_Main._batchedAllocationSize).c_str());
                // OutputDebugString(FormatString("   Commit Step Empty: (%i)\n", context.GetCommitStepUnderConstruction().IsEmpty()).c_str());
                // OutputDebugString(FormatString("   Publishable event list: (%i)\n", publishableEventList).c_str());
                // OutputDebugString(FormatString("   Commit count current: (%i)\n", commitCountCurrent).c_str());
                // OutputDebugString(FormatString("   Commit count last resolve: (%i)\n", context.CommitCount_LastResolve()).c_str());
                // OutputDebugString(FormatString("   Frame priority: (%i)\n", framePriorityResolve).c_str());
                // OutputDebugString(FormatString("   From frame priority: (%i)\n", fromFramePriorityQueueSet).c_str());

                context.CommitCount_LastResolve() = commitCountCurrent;

                    //
                    //      Flush through the delayed releases again. Let's try to hit a low water mark before we allocate
                    //      from the batched buffers
                    //
                publishableEventList = TickResourceSource(stepMask, context, isLoading);
#if BU_BATCHING
                ResolveBatchOperation(_batchPreparation_Main, context, stepMask);
                _batchPreparation_Main = BatchPreparation();
#endif
                metricsUnderConstruction._assemblyLineMetrics = CalculateMetrics();

                context.ResolveCommandList();
                context.EventList_Publish(publishableEventList);

                atLeastOneRealAction = true;
            }

            if (commandListIdCommitted != ~unsigned(0x0)) {
                    //
                    //      Don't cross this barrier until the next command list is finished
                    //
                while (!_resourceSource.MarkBarrier(commandListIdCommitted)) {
                    _resourceSource.FlushDelayedReleases(context.CommandList_GetCompletedByGPU());
                    Threading::YieldTimeSlice();
                }
            }

            #if defined(_DEBUG)
                if (framePriorityResolve) {
                    ScopedLock(_transactionsToBeCompletedNextFramePriorityCommit_Lock);
                    for (   auto i =_transactionsToBeCompletedNextFramePriorityCommit.begin(); 
                                 i!=_transactionsToBeCompletedNextFramePriorityCommit.end(); ++i) {
                        Transaction* transaction = GetTransaction(*i);
                        if (transaction) {
                            auto referenceCount = transaction->_referenceCount.load();
                            const bool isCompleted = ((referenceCount & 0x00ffffff) == 0)
                                &&  (transaction->_retirementCommandList <= commandListIdCommitted);
                            assert(isCompleted);
                        }
                    }
                    _transactionsToBeCompletedNextFramePriorityCommit.clear();
                }
            #endif

            if (framePriorityResolve) {
                context._pendingFramePriority_CommandLists.pop();
#if BU_BATCHING
                assert(!_batchPreparation_Main._batchedAllocationSize);
#endif
            }

                /////////////// ~~~~ /////////////// ~~~~ ///////////////
            if (!atLeastOneRealAction) {
                // publishableEventList = TickResourceSource(stepMask, context, isLoading);
                _queuedWorkFlag = !nothingFoundInQueues;
                break;
            }

            Threading::YieldTimeSlice();
        }
    }

    PoolSystemMetrics   AssemblyLine::CalculatePoolMetrics() const
    {
        return _resourceSource.CalculatePoolMetrics();
    }

    AssemblyLineMetrics AssemblyLine::CalculateMetrics()
    {
        AssemblyLineMetrics result;
        result._queuedPrepareStaging            = (unsigned)_queueSet_Main._prepareStagingSteps.size();
        result._queuedTransferStagingToFinal    = (unsigned)_queueSet_Main._transferStagingToFinalSteps.size();
        result._queuedCreateFromDataPacket      = (unsigned)_queueSet_Main._createFromDataPacketSteps.size();
        for (unsigned c=0; c<dimof(_queueSet_FramePriority); ++c) {
            result._queuedPrepareStaging            += (unsigned)_queueSet_FramePriority[c]._prepareStagingSteps.size();
            result._queuedTransferStagingToFinal    += (unsigned)_queueSet_FramePriority[c]._transferStagingToFinalSteps.size();
            result._queuedCreateFromDataPacket      += (unsigned)_queueSet_FramePriority[c]._createFromDataPacketSteps.size();
        }
        _peakPrepareStaging = result._peakPrepareStaging = std::max(_peakPrepareStaging, result._queuedPrepareStaging);
        _peakTransferStagingToFinal = result._peakTransferStagingToFinal = std::max(_peakTransferStagingToFinal, result._queuedTransferStagingToFinal);
        _peakCreateFromDataPacket = result._peakCreateFromDataPacket = std::max(_peakCreateFromDataPacket, result._queuedCreateFromDataPacket);
        std::copy(_currentQueuedBytes, &_currentQueuedBytes[(unsigned)UploadDataType::Max], result._queuedBytes);

            //
            //      calculating the transaction count is the most expensive part...
            //      we need to check for all transactions with a high reference count.
            //      but the transactions list can get very big...!
            //
        static unsigned callCount = 0;
        ++callCount;
        if (!(callCount%30)) {
            {
                #if !defined(DEQUE_BASED_TRANSACTIONS) && !defined(OPTIMISED_ALLOCATE_TRANSACTION)
                    ScopedLock(_transactionsLock);

                    {
                            //  this version will shrink the list safely... (temporary first...)
                        std::vector<Transaction>::const_reverse_iterator begini=_transactions.rend()-_transactions_TemporaryCount;
                        std::vector<Transaction>::const_reverse_iterator endi=_transactions.rend();
                        std::vector<Transaction>::const_reverse_iterator lastValidTransaction = endi;
                        for (std::vector<Transaction>::const_reverse_iterator i=begini; i!=endi; ++i) {
                            if (i->_referenceCount > 0) {
                                lastValidTransaction = i;
                                break;
                            }
                        }
                            //      note -- not calling destructor on transactions when they are erased
                        if (lastValidTransaction == endi) {
                            _transactions_TemporaryCount = 0;
                        } else {
                            _transactions_TemporaryCount = std::distance(lastValidTransaction, endi);
                        }
                    }

                    {
                            //  now shrink long term
                        std::vector<Transaction>::const_iterator begini = _transactions.end()-_transactions_LongTermCount;
                        std::vector<Transaction>::const_iterator endi = _transactions.end();
                        std::vector<Transaction>::const_iterator lastValidTransaction = endi;
                        for (std::vector<Transaction>::const_iterator i=begini; i!=endi; ++i) {
                            if (i->_referenceCount > 0) {
                                lastValidTransaction = i;
                                break;
                            }
                        }
                            //      note -- not calling destructor on transactions when they are erased
                        if (lastValidTransaction == endi) {
                            _transactions_LongTermCount = 0;
                        } else {
                            _transactions_LongTermCount = std::distance(lastValidTransaction, endi);
                        }
                    }
                #endif
            }
        }
        result._transactionCount                 = _allocatedTransactionCount;
        #if defined(DEQUE_BASED_TRANSACTIONS)
            result._temporaryTransactionsAllocated   = (unsigned)_transactions.size();
            result._longTermTransactionsAllocated    = (unsigned)_transactions_LongTerm.size();
        #else
            result._temporaryTransactionsAllocated   = _transactions_TemporaryCount;
            result._longTermTransactionsAllocated    = _transactions_LongTermCount;
        #endif
        // #if defined(OPTIMISED_ALLOCATE_TRANSACTION)
        //     assert(_allocatedTransactionCount == (_transactionsHeap.CalculateAllocatedSpace() >> 4));
        // #endif
        return result;
    }

#if 0
    void AssemblyLine::UpdateData(TransactionID id, DataPacket* rawData, const PartialResource& part)
    {
        Transaction* transaction = GetTransaction(id);
        assert(transaction);

        #if defined(_DEBUG)&&defined(DIRECT3D9)
            if (transaction->_desc._type == ResourceDesc::Type::Texture) {
                assert(transaction->_desc._textureDesc._nativePixelFormat != 0);
            }
        #endif

        if (rawData) {
            auto bkgrndMarker = rawData->BeginBackgroundLoad();
            if (bkgrndMarker) {
                // this operation involves a background load. We need queue the background load first
                // After the background load is completed, the rest of the operation will continue
                PushStep(GetQueueSet(transaction->_creationOptions), *transaction, PrepareDataStep(id, rawData, std::move(bkgrndMarker), part));
                return;
            }
        }

        UpdateData_PostBackground(GetQueueSet(transaction->_creationOptions), *transaction, id, rawData, part);
    }

    void AssemblyLine::UpdateData_PostBackground(QueueSet& queueSet, Transaction& transaction, TransactionID id, DataPacket* rawData, const PartialResource& part)
    {
            //  
            //      1. Queue creation & staging creation steps. But only if these haven't been queued or completed before.
            //      2. Queue an upload step
            //
            //      Because we might be running on multiple threads, it's difficult to know whether the creation
            //      steps have been queued or completed in a way that is safe from race conditions. So lets just use
            //      some boolean flags protected by a low-overhead lock.
            //
        unsigned requestedStagingLODOffset = 0;
        if (transaction._desc._type == ResourceDesc::Type::Texture) {
            unsigned maxLodOffset = IntegerLog2(std::min(transaction._desc._textureDesc._width, transaction._desc._textureDesc._height))-2;
            requestedStagingLODOffset = std::min(part._lodLevelMin, maxLodOffset);
        }

        assert(transaction._desc._type != ResourceDesc::Type::Unknown);

        bool mustQueueCreation = false, mustQueueStaging = false;
        {
                //  Simple busy loop lock for these few booleans
            while (Interlocked::CompareExchange(&transaction._statusLock, 1, 0)!=0) {Threading::Pause();}

            mustQueueCreation = transaction._creationQueued == false;
            transaction._creationQueued = true;
            const bool uploadViaStaging = PlatformInterface::RequiresStagingTextureUpload && (transaction._desc._type == ResourceDesc::Type::Texture) && (transaction._desc._allocationRules != AllocationRules::NonVolatile);
            if (uploadViaStaging) {
                mustQueueStaging = (transaction._stagingQueued == false) || (transaction._requestedStagingLODOffset!=requestedStagingLODOffset);
                transaction._stagingQueued = true;
            }

            auto lockRelease = Interlocked::Exchange(&transaction._statusLock, 0);
            assert(lockRelease==1); (void)lockRelease;
        }

        #if defined(_DEBUG)
                //
                //      Validate the size of information in the initialisation packet.
                //
            if (rawData && transaction._desc._type == ResourceDesc::Type::Texture 
                && (!part._box._left && !part._box._top && !part._box._right && !part._box._bottom)) {
                for (unsigned m=0; m<transaction._desc._textureDesc._mipCount; ++m) {
                    const size_t dataSize = rawData->GetDataSize(SubR(m, 0));
                    if (dataSize) {
                        TextureDesc mipMapDesc     = RenderCore::CalculateMipMapDesc(transaction._desc._textureDesc, m);
                        mipMapDesc._mipCount       = 1;
                        mipMapDesc._arrayCount     = 1;
                        const size_t expectedSize  = RenderCore::ByteCount(mipMapDesc);
                        assert(std::max(size_t(16),dataSize) == std::max(size_t(16),expectedSize));
                    }
                }
            }
        #endif

        const bool initializeOnCreation = mustQueueCreation && !mustQueueStaging && part._arrayIndexMax== 0 && part._arrayIndexMin==0;
        if (initializeOnCreation) {
            PushStep(queueSet, transaction, ResourceCreateStep(id, transaction._desc, rawData));
        } else {
            if (mustQueueCreation)
                PushStep(queueSet, transaction, ResourceCreateStep(id, transaction._desc));
            if (mustQueueStaging) {
                transaction._requestedStagingLODOffset = requestedStagingLODOffset;
                PushStep_StagingBuffer(queueSet, transaction, ResourceCreateStep(id, transaction._desc));
            }

                // there is a separate "step" for each array element
            for (unsigned e = part._arrayIndexMin; e <= part._arrayIndexMax; ++e)
                PushStep(queueSet, transaction, DataUploadStep(id, rawData, part._box, part._lodLevelMin, part._lodLevelMax, e));
        }

        unsigned size = 0;
		if (rawData)
			for (unsigned l = part._lodLevelMin; l <= part._lodLevelMax; ++l)
                for (unsigned e = part._arrayIndexMin; e <= part._arrayIndexMax; ++e)
				    size += (unsigned)rawData->GetDataSize(SubR(l, e));

        if (transaction._desc._type == ResourceDesc::Type::LinearBuffer) {
            assert(RenderCore::ByteCount(transaction._desc)==unsigned(size));
        }
        _currentQueuedBytes[(unsigned)AsUploadDataType(transaction._desc)] += size;
    }
#endif

    ResourceLocator     AssemblyLine::GetResource(TransactionID id)
    {
        ScopedLock(_transactionsRepositionLock);
        Transaction* transaction = GetTransaction(id);
        if (transaction) {
            return transaction->_finalResource;
        }
        return {};
    }

    void                    AssemblyLine::Resource_Validate(const ResourceLocator& locator)
    {
        _resourceSource.Validate(locator);
    }

    AssemblyLine::QueueSet& AssemblyLine::GetQueueSet(TransactionOptions::BitField transactionOptions)
    {
        if (transactionOptions & TransactionOptions::FramePriority) {
            return _queueSet_FramePriority[_framePriority_WritingQueueSet];    // not 100% thread safe
        } else {
            return _queueSet_Main;
        }
    }

    void AssemblyLine::PushStep(QueueSet& queueSet, Transaction& transaction, PrepareStagingStep&& step)
    {
        ++transaction._referenceCount;
        queueSet._prepareStagingSteps.push(std::move(step));
        _wakeupEvent.Increment();
    }

    void AssemblyLine::PushStep(QueueSet& queueSet, Transaction& transaction, TransferStagingToFinalStep&& step)
    {
        ++transaction._referenceCount;
        queueSet._transferStagingToFinalSteps.push(std::move(step));
        _wakeupEvent.Increment();
    }

    void AssemblyLine::PushStep(QueueSet& queueSet, Transaction& transaction, CreateFromDataPacketStep&& step)
    {
        ++transaction._referenceCount;
        queueSet._createFromDataPacketSteps.push(std::move(step));
        _wakeupEvent.Increment();
    }

    unsigned AssemblyLine::FlipWritingQueueSet()
    {
            //      This works best if we're only accessing _currentFramePriorityQueueSet from a single
            //      thread. Eg; we should schedule operations for frame priority transactions from the 
            //      main thread, and set the barrier at the end of the main thread;
        unsigned oldWritingQueueSet = _framePriority_WritingQueueSet;
        _framePriority_WritingQueueSet = (_framePriority_WritingQueueSet+1)%dimof(_queueSet_FramePriority);
        return oldWritingQueueSet;
    }

        ///////////////////   M A N A G E R   ///////////////////

    void                    Manager::UpdateData(TransactionID id, const std::shared_ptr<IDataPacket>& data, const PartialResource& part)
    {
        assert(0);
        // _assemblyLine->UpdateData(id, data, part);
    }

    TransactionMarker           Manager::Transaction_Begin(const ResourceDesc& desc, const std::shared_ptr<IDataPacket>& data, TransactionOptions::BitField flags)
    {
        return _assemblyLine->Transaction_Begin(desc, data, flags);
    }

    TransactionMarker           Manager::Transaction_Begin(const std::shared_ptr<IAsyncDataSource>& data, TransactionOptions::BitField flags)
    {
        return _assemblyLine->Transaction_Begin(data, flags);
    }

    TransactionMarker           Manager::Transaction_Begin(const ResourceLocator& locator, TransactionOptions::BitField flags)
    {
        return _assemblyLine->Transaction_Begin(locator, flags);
    }

    ResourceLocator         Manager::GetResource(TransactionID id)
    {
        return _assemblyLine->GetResource(id);
    }

    void                    Manager::Resource_Validate(const ResourceLocator& locator)
    {
        _assemblyLine->Resource_Validate(locator);
    }

        /////////////////////////////////////////////

    void                    Manager::Transaction_Cancel(TransactionID id)
    {
        _assemblyLine->Transaction_Cancel(id);
    }

    void                    Manager::Transaction_Validate(TransactionID id)
    {
        _assemblyLine->Transaction_Validate(id);
    }

    ResourceLocator         Manager::Transaction_Immediate(
        RenderCore::IThreadContext& threadContext,
        const ResourceDesc& desc, IDataPacket& data,
        const PartialResource& part)
    {
        return _assemblyLine->Transaction_Immediate(threadContext, desc, data, part);
    }

    /*void                    Manager::AddRef(TransactionID id)
    {
        _assemblyLine->Transaction_AddRef(id);
    }*/

    inline ThreadContext*          Manager::MainContext() 
    { 
        return _backgroundStepMask ? _backgroundContext.get() : _foregroundContext.get(); 
    }

    inline const ThreadContext*          Manager::MainContext() const
    { 
        return _backgroundStepMask ? _backgroundContext.get() : _foregroundContext.get(); 
    }

    bool                    Manager::IsCompleted(TransactionID id)
    {
        return _assemblyLine->IsCompleted(id, MainContext()->CommandList_GetCommittedToImmediate());
    }

    CommandListMetrics      Manager::PopMetrics()
    {
        CommandListMetrics result = _backgroundContext->PopMetrics();
        if (result._commitTime != 0x0) {
            return result;
        }
        return _foregroundContext->PopMetrics();
    }

    PoolSystemMetrics       Manager::CalculatePoolMetrics() const
    {
        return _assemblyLine->CalculatePoolMetrics();
    }

    size_t                  Manager::ByteCount(const ResourceDesc& desc) const
    {
        return RenderCore::ByteCount(desc);
    }

    Manager::EventListID    Manager::EventList_GetLatestID()
    {
        if (_backgroundStepMask&AssemblyLine::Step_BatchedDefrag) {
            return _backgroundContext->EventList_GetPublishedID();
        }
        return _foregroundContext->EventList_GetPublishedID();
    }

    void                    Manager::EventList_Get(EventListID id, Event_ResourceReposition*& begin, Event_ResourceReposition*& end)
    {
        if (_backgroundStepMask&AssemblyLine::Step_BatchedDefrag) {
            return _backgroundContext->EventList_Get(id, begin, end);
        }
        return _foregroundContext->EventList_Get(id, begin, end);
    }

    void                    Manager::EventList_Release(EventListID id)
    {
        if (_backgroundStepMask&AssemblyLine::Step_BatchedDefrag) {
            return _backgroundContext->EventList_Release(id);
        }
        return _foregroundContext->EventList_Release(id);
    }

    void                    Manager::Update(RenderCore::IThreadContext& immediateContext, bool preserveRenderState)
    {
        if (_foregroundStepMask & ~unsigned(AssemblyLine::Step_BatchingUpload)) {
            _assemblyLine->Process(_foregroundStepMask, *_foregroundContext.get());
        }
            //  Commit both the foreground and background contexts here
        _foregroundContext->CommitToImmediate(immediateContext, *_gpuEventStack, preserveRenderState);
        _backgroundContext->CommitToImmediate(immediateContext, *_gpuEventStack, preserveRenderState);

        PlatformInterface::Resource_RecalculateVideoMemoryHeadroom();
    }

    void                    Manager::Flush()
    {
        while (_assemblyLine->QueuedWork()) {
            // Update();
            Threading::YieldTimeSlice();
        }
    }

    void Manager::FramePriority_Barrier()
    {
        unsigned oldQueueSetId = _assemblyLine->FlipWritingQueueSet();
        if (_backgroundStepMask) {
            MainContext()->FramePriority_Barrier(oldQueueSetId);
        }
    }

    uint32_t Manager::DoBackgroundThread()
    {
        if (_backgroundContext) {
            _backgroundContext->BeginCommandList();
        }

        while (!_shutdownBackgroundThread && _backgroundStepMask) {
            if (!_shutdownBackgroundThread) {
                _assemblyLine->Process(_backgroundStepMask, *_backgroundContext);
            }
            if (!_shutdownBackgroundThread) {
                _assemblyLine->Wait(_backgroundStepMask, *_backgroundContext);
            }
        }
        return 0;
    }

    Manager::Manager(RenderCore::IDevice& renderDevice) : _assemblyLine(std::make_shared<AssemblyLine>(renderDevice))
    {
        _shutdownBackgroundThread = false;

        bool multithreadingOk = true; // CRenderer::CV_r_BufferUpload_Enable!=2;
        bool doBatchingUploadInForeground = !PlatformInterface::CanDoNooverwriteMapInBackground;

        const auto nsightMode = ConsoleRig::CrossModule::GetInstance()._services.CallDefault(Hash64("nsight"), false);
        if (nsightMode)
            multithreadingOk = false;

        auto immediateDeviceContext = renderDevice.GetImmediateContext();
        decltype(immediateDeviceContext) backgroundDeviceContext;

        if (multithreadingOk) {
            backgroundDeviceContext = renderDevice.CreateDeferredContext();

                //
                //      When using an older feature level, we can fail while
                //      creating a deferred context. In these cases, we have
                //      to drop back to single threaded mode.
                //
            if (!backgroundDeviceContext) {
                backgroundDeviceContext = immediateDeviceContext;
            }
        } else {
            backgroundDeviceContext = immediateDeviceContext;
        }

        multithreadingOk = !backgroundDeviceContext->IsImmediate() && (backgroundDeviceContext != immediateDeviceContext);
        _backgroundContext   = std::make_unique<ThreadContext>(backgroundDeviceContext);
        _foregroundContext   = std::make_unique<ThreadContext>(std::move(immediateDeviceContext));
        _gpuEventStack       = std::make_unique<PlatformInterface::GPUEventStack>(renderDevice);

            //  todo --     if we don't have driver support for concurrent creates, we should try to do this
            //              in the main render thread. Also, if we've created the device with the single threaded
            //              parameter, we should do the same.

        if (multithreadingOk) {
            _foregroundStepMask = doBatchingUploadInForeground?AssemblyLine::Step_BatchingUpload:0;        // (do this with the immediate context (main thread) in order to allow writing directly to video memory
            _backgroundStepMask = 
                    AssemblyLine::Step_PrepareStaging
                |   AssemblyLine::Step_TransferStagingToFinal
                |   AssemblyLine::Step_CreateFromDataPacket
                |   AssemblyLine::Step_DelayedReleases
                |   AssemblyLine::Step_BatchedDefrag
                |   ((!doBatchingUploadInForeground)?AssemblyLine::Step_BatchingUpload:0)
                ;
        } else {
            _foregroundStepMask = 
                    AssemblyLine::Step_PrepareStaging
                |   AssemblyLine::Step_TransferStagingToFinal
                |   AssemblyLine::Step_CreateFromDataPacket
                |   AssemblyLine::Step_BatchingUpload
                |   AssemblyLine::Step_DelayedReleases
                |   AssemblyLine::Step_BatchedDefrag
                ;
            _backgroundStepMask = 0;
        }
        if (_backgroundStepMask) {
            _backgroundThread = std::make_unique<std::thread>([this](){ return DoBackgroundThread(); });
        }
    }

    Manager::~Manager()
    {
        _shutdownBackgroundThread = true;       // this will cause the background thread to terminate at it's next opportunity
        _assemblyLine->TriggerWakeupEvent();
        if (_backgroundThread) {
            _backgroundThread->join();
        }
    }

    std::unique_ptr<IManager> CreateManager(RenderCore::IDevice& renderDevice)
    {
        return std::make_unique<Manager>(renderDevice);
    }

    #if OUTPUT_DLL
        static ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> s_attachRef;
        void AttachLibrary(ConsoleRig::CrossModule& globalServices)
        {
			ConsoleRig::CrossModule::SetInstance(crossModule);
            s_attachRef = ConsoleRig::GetAttachablePtr<ConsoleRig::GlobalServices>();
			auto versionDesc = ConsoleRig::GetLibVersionDesc();
			Log(Verbose) << "Attached Buffer Uploads DLL: {" << versionDesc._versionString << "} -- {" << versionDesc._buildDateString << "}" << std::endl;
        }

        void DetachLibrary()
        {
            s_attachRef.reset();
			ConsoleRig::CrossModule::ReleaseInstance();
        }
    #else
        void AttachLibrary(ConsoleRig::CrossModule& globalServices)
        {
            assert(&globalServices == &ConsoleRig::CrossModule::GetInstance());
        }
        void DetachLibrary() {}
    #endif

    IManager::~IManager() {}
}


