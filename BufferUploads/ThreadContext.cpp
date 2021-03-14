// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ThreadContext.h"
#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../OSServices/WinAPI/System_WinAPI.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/HeapUtils.h"

#pragma warning(disable:4127) // conditional expression is constant

namespace BufferUploads
{
    CommandList::CommandList()  {}
    CommandList::~CommandList() {}

    CommandList::CommandList(CommandList&& moveFrom)
    : _deviceCommandList(std::move(moveFrom._deviceCommandList))
    , _metrics(std::move(moveFrom._metrics))
    , _commitStep(std::move(moveFrom._commitStep))
    , _id(moveFrom._id)
    {}

    CommandList& CommandList::operator=(CommandList&& moveFrom)
    {
        _deviceCommandList = std::move(moveFrom._deviceCommandList);
        _metrics = std::move(moveFrom._metrics);
        _commitStep = std::move(moveFrom._commitStep);
        _id = moveFrom._id;
        return *this;
    }

    void ThreadContext::BeginCommandList()
    {
        _deviceContext.BeginCommandList();
    }

    void ThreadContext::ResolveCommandList()
    {
        int64_t currentTime = PlatformInterface::QueryPerformanceCounter();
        CommandList newCommandList;
        newCommandList._metrics = _commandListUnderConstruction;
        newCommandList._metrics._resolveTime = currentTime;
        newCommandList._metrics._processingEnd = currentTime;
        newCommandList._id = _commandListIDUnderConstruction;

        if (_requiresResolves) {
            newCommandList._deviceCommandList = _deviceContext.ResolveCommandList();
            newCommandList._commitStep.swap(_commitStepUnderConstruction);
            _queuedCommandLists.push_overflow(std::move(newCommandList));
        } else {
                    // immediate resolve -- skip the render thread resolve step...
            _commitStepUnderConstruction.CommitToImmediate_PreCommandList(*_underlyingContext);
            _commitStepUnderConstruction.CommitToImmediate_PostCommandList(*_underlyingContext);
            newCommandList._metrics._frameId = _underlyingContext->GetStateDesc()._frameId;
            newCommandList._metrics._commitTime = currentTime;
            #if defined(XL_BUFFER_UPLOAD_RECORD_THREAD_CONTEXT_METRICS)
                while (!_recentRetirements.push(newCommandList._metrics)) {
                    _recentRetirements.pop();   // note -- this might violate the single-popping-thread rule!
                }
            #endif
            _commandListIDCommittedToImmediate = std::max(_commandListIDCommittedToImmediate, _commandListIDUnderConstruction);
        }

        _commandListUnderConstruction = CommandListMetrics();
        _commandListUnderConstruction._processingStart = currentTime;
        CommitStep().swap(_commitStepUnderConstruction);
        ++_commandListIDUnderConstruction;
    }

    void ThreadContext::CommitToImmediate(
        RenderCore::IThreadContext& commitTo,
        PlatformInterface::GPUEventStack& gpuEventStack,
        bool preserveRenderState)
    {
        auto immContext = RenderCore::Metal::DeviceContext::Get(commitTo);
        if (_requiresResolves) {
            // FUNCTION_PROFILER_RENDER_FLAT

            TimeMarker stallStart = PlatformInterface::QueryPerformanceCounter();

            bool gotStart = false;
            for (;;) {

                    //
                    //      While there are uncommitted frame-priority command lists, we need to 
                    //      stall to wait until they are committed. Keep trying to drain the queue
                    //      until there are no lists, and nothing pending.
                    //

                const bool currentlyUncommitedFramePriorityCommandLists = _pendingFramePriority_CommandLists.size()!=0;

                CommandList* commandList = 0;
                while (_queuedCommandLists.try_front(commandList)) {
                    TimeMarker stallEnd = PlatformInterface::QueryPerformanceCounter();
                    if (!gotStart) {
                        // PROFILE_LABEL_PUSH("GPU_UPLOAD");
                        // GPU_TIMER_START("GPU_UPLOAD");
                        gotStart = true;
                    }

                    commandList->_commitStep.CommitToImmediate_PreCommandList(commitTo);
                    if (commandList->_deviceCommandList) {
                        immContext->ExecuteCommandList(*commandList->_deviceCommandList.get(), preserveRenderState);
                    }
                    commandList->_commitStep.CommitToImmediate_PostCommandList(commitTo);
                    _commandListIDCommittedToImmediate   = std::max(_commandListIDCommittedToImmediate, commandList->_id);
                    gpuEventStack.TriggerEvent(immContext.get(), commandList->_id);
                
                    commandList->_metrics._frameId                  = commitTo.GetStateDesc()._frameId;
                    commandList->_metrics._commitTime               = PlatformInterface::QueryPerformanceCounter();
                    commandList->_metrics._framePriorityStallTime   = stallEnd - stallStart;    // this should give us very small numbers, when we're not actually stalling for frame priority commits
                    #if defined(XL_BUFFER_UPLOAD_RECORD_THREAD_CONTEXT_METRICS)
                        while (!_recentRetirements.push(commandList->_metrics)) {
                            _recentRetirements.pop();   // note -- this might violate the single-popping-thread rule!
                        }
                    #endif
                    _queuedCommandLists.pop();

                    stallStart = PlatformInterface::QueryPerformanceCounter();                
                }
                    
                if (!currentlyUncommitedFramePriorityCommandLists) {
                    break;
                }

                Threading::YieldTimeSlice();
                gpuEventStack.Update(immContext.get());
                _commandListIDCompletedByGPU = gpuEventStack.GetLastCompletedEvent();   // update the GPU progress, incase we get stuck here for awhile
            }

            if (gotStart) {
                // GPU_TIMER_STOP("GPU_UPLOAD");
                // PROFILE_LABEL_POP("GPU_UPLOAD");
            }
        } else {
            if (_commandListIDCommittedToImmediate) {
                gpuEventStack.TriggerEvent(immContext.get(), _commandListIDCommittedToImmediate);
            }
            while (_pendingFramePriority_CommandLists.size()!=0) {
                Threading::YieldTimeSlice();
                gpuEventStack.Update(immContext.get());
                _commandListIDCompletedByGPU = gpuEventStack.GetLastCompletedEvent();
            }
        }

        ++_commitCountCurrent;
        gpuEventStack.Update(immContext.get());
        _commandListIDCompletedByGPU = gpuEventStack.GetLastCompletedEvent();
        XlSetEvent(_wakeupEvent);   // wake up the background thread -- it might be time for a resolve
    }

    CommandListMetrics ThreadContext::PopMetrics()
    {
        #if defined(XL_BUFFER_UPLOAD_RECORD_THREAD_CONTEXT_METRICS)
            CommandListMetrics* ptr;
            if (_recentRetirements.try_front(ptr)) {
                CommandListMetrics result = *ptr;
                _recentRetirements.pop();
                return result;
            }
        #endif
        return CommandListMetrics();
    }

    IManager::EventListID   ThreadContext::EventList_GetWrittenID() const
    {
        return _currentEventListId;
    }

    IManager::EventListID   ThreadContext::EventList_GetPublishedID() const
    {
        return _currentEventListPublishedId;
    }

    IManager::EventListID   ThreadContext::EventList_GetProcessedID() const
    {
        return _currentEventListProcessedId;
    }

    void                    ThreadContext::EventList_Get(IManager::EventListID id, Event_ResourceReposition*& begin, Event_ResourceReposition*& end)
    {
        begin = end = NULL;
        if (!id) return;
        for (unsigned c=0; c<dimof(_eventBuffers); ++c) {
            if (_eventBuffers[c]._id == id) {
                Interlocked::Increment(&_eventBuffers[c]._clientReferences);
                    //  have to check again after the increment... because the client references value acts
                    //  as a lock.
                if (_eventBuffers[c]._id == id) {
                    begin = &_eventBuffers[c]._evnt;
                    end = begin+1;
                } else {
                    Interlocked::Decrement(&_eventBuffers[c]._clientReferences);
                        // in this case, the event has just be freshly overwritten
                }
                return;
            }
        }
        //      DavidJ --   sometimes we're getting here; it appears we just have empty
        //                  event lists sometimes
        // assert(0);
    }

    void                    ThreadContext::EventList_Release(IManager::EventListID id, bool silent)
    {
        if (!id) return;
        for (unsigned c=0; c<dimof(_eventBuffers); ++c) {
            if (_eventBuffers[c]._id == id) {
                assert(_eventBuffers[c]._clientReferences);
                Interlocked::Decrement(&_eventBuffers[c]._clientReferences);
                    
                if (!silent) {
                    for (;;) {      // lock-free max...
                        Interlocked::Value originalProcessedId = _currentEventListProcessedId;
                        Interlocked::Value newProcessedId = std::max(originalProcessedId, Interlocked::Value(_eventBuffers[c]._id));
                        Interlocked::Value beforeExchange = Interlocked::CompareExchange(&_currentEventListProcessedId, newProcessedId, originalProcessedId);
                        if (beforeExchange == originalProcessedId) {
                            break;
                        }
                    }
                }
                return;
            }
        }
        // assert(0);
    }

    IManager::EventListID   ThreadContext::EventList_Push(const Event_ResourceReposition& evnt)
    {
            //
            //      try to push this event into the small queue... but don't overwrite anything that
            //      currently has a client reference on it.
            //
        if (!_eventBuffers[_eventListWritingIndex]._clientReferences) {
            IManager::EventListID id = ++_currentEventListId;
            _eventBuffers[_eventListWritingIndex]._id = id;
            _eventBuffers[_eventListWritingIndex]._evnt = evnt;
            _eventListWritingIndex = (_eventListWritingIndex+1)%dimof(_eventBuffers);   // single writing thread, so it's ok
            return id;
        }
        assert(0);
        return ~IManager::EventListID(0x0);
    }

    void ThreadContext::EventList_Publish(IManager::EventListID toEvent)
    {
        _currentEventListPublishedId = toEvent;
    }

    void ThreadContext::FramePriority_Barrier(unsigned queueSetId)
    {
            //      Since we're only double buffering, we can't continue until the  
            //      we finish with the previous priority steps...
        // unsigned commandListId = CommandList_GetUnderConstruction();
        while (!_pendingFramePriority_CommandLists.push(queueSetId)) {
            XlSetEvent(_wakeupEvent);
            Threading::YieldTimeSlice(); 
        }
        XlSetEvent(_wakeupEvent);
    }
    
    void ThreadContext::OnLostDevice()
    {
        for (unsigned c=0; c<dimof(_eventBuffers); ++c) {
                //      Clear out these pointers because the things they point to have probably
                //      been destroyed.
            _eventBuffers[c]._evnt._newResource = 0;
            _eventBuffers[c]._evnt._originalResource = 0;
            _eventBuffers[c]._evnt._defragSteps.clear();
            _eventBuffers[c]._id = 0;
        }
    }

    ThreadContext::ThreadContext(std::shared_ptr<RenderCore::IThreadContext> underlyingContext) 
    : _deviceContext(*underlyingContext), _requiresResolves(PlatformInterface::ContextBasedMultithreading) // context != PlatformInterface::GetImmediateContext())
    , _currentEventListId(0), _eventListWritingIndex(0), _currentEventListProcessedId(0)
    , _currentEventListPublishedId(0)
    {
        _underlyingContext = std::move(underlyingContext);
        _lastResolve = _tickFrequency = 0;
        _commitCountCurrent = _commitCountLastResolve = 0;
        // XlZeroMemory(_eventBuffers);
        #if defined(WIN32) || defined(WIN64)
            QueryPerformanceFrequency((LARGE_INTEGER*)&_tickFrequency);
        #endif

        if (_underlyingContext->IsImmediate()) {
            _requiresResolves = false;  // immediate context requires no resolves
        }

        // for (unsigned c=0; c<dimof(_eventBuffers); ++c) {
        //     _eventBuffers[c]._id = ~IManager::EventListID(0x0);
        // }

        _commandListIDUnderConstruction = 1;
        _commandListIDCompletedByGPU = 0;
        _commandListIDCommittedToImmediate = 0;

        _wakeupEvent = XlCreateEvent(false);
    }

    ThreadContext::~ThreadContext()
    {
        XlCloseSyncObject(_wakeupEvent);
    }

        //////////////////////////////////////////////////////////////////////////////////////////////

    CommitStep::DeferredCopy::DeferredCopy()
    {
        _size = 0;
    }

    CommitStep::DeferredCopy::DeferredCopy(const ResourceLocator& destination, unsigned size, std::shared_ptr<IDataPacket> pkt)
    : _destination(destination), _size(size), _temporaryBuffer(std::move(pkt))
    {
    }

    CommitStep::DeferredCopy::DeferredCopy(DeferredCopy&& moveFrom)
    : _destination(std::move(moveFrom._destination)), _size(moveFrom._size), _temporaryBuffer(moveFrom._temporaryBuffer)
    {
        moveFrom._temporaryBuffer = nullptr;
    }

    const CommitStep::DeferredCopy& CommitStep::DeferredCopy::operator=(DeferredCopy&& moveFrom)
    {
        _destination = std::move(moveFrom._destination);
        _size = moveFrom._size;
        _temporaryBuffer = std::move(moveFrom._temporaryBuffer);
        moveFrom._temporaryBuffer = NULL;
        return *this;
    }

    CommitStep::DeferredCopy::~DeferredCopy()
    {
    }

    CommitStep::DeferredDefragCopy::DeferredDefragCopy(
		std::shared_ptr<IResource> destination, std::shared_ptr<IResource> source, const std::vector<DefragStep>& steps)
    : _destination(std::move(destination)), _source(std::move(source)), _steps(steps)
    {}

    CommitStep::DeferredDefragCopy::~DeferredDefragCopy()
    {}

    void CommitStep::Add(CommitStep::DeferredCopy&& copy)
    {
        _deferredCopies.push_back(std::forward<CommitStep::DeferredCopy>(copy));
    }

    void CommitStep::Add(CommitStep::DeferredDefragCopy&& copy)
    {
        _deferredDefragCopies.push_back(std::forward<CommitStep::DeferredDefragCopy>(copy));
    }

    void CommitStep::CommitToImmediate_PreCommandList(RenderCore::IThreadContext& immContext)
    {
        if (!_deferredCopies.empty()) {
			assert(0);	// this functionality not implemented for Vulkan compatibility
			#if 0
				PlatformInterface::UnderlyingDeviceContext immediateContext(immContext);
				for (auto i=_deferredCopies.begin(); i!=_deferredCopies.end(); ++i) {
					const bool useMapPath = true;
					if (useMapPath) {
						auto mappedBuffer = immediateContext.Map(*i->_destination->GetUnderlying(), PlatformInterface::UnderlyingDeviceContext::MapType::NoOverwrite);
						XlCopyMemoryAlign16(PtrAdd(mappedBuffer.GetData(), i->_destination->Offset()), i->_temporaryBuffer->GetData(), i->_size);
					} else {
						immediateContext.PushToResource(
							*i->_destination->GetUnderlying(), ResourceDesc(), i->_destination->Offset(), i->_temporaryBuffer->GetData(), i->_size, TexturePitches(), Box2D(), 0, 0);
					}
				}
			#endif
        }
    }

    void CommitStep::CommitToImmediate_PostCommandList(RenderCore::IThreadContext& immContext)
    {
        if (!_deferredDefragCopies.empty()) {
            PlatformInterface::UnderlyingDeviceContext immediateContext(immContext);
            for (auto i=_deferredDefragCopies.begin(); i!=_deferredDefragCopies.end(); ++i)
                immediateContext.ResourceCopy_DefragSteps(i->_destination, i->_source, i->_steps);
        }
    }

    bool CommitStep::IsEmpty() const 
    {
        return _deferredCopies.empty() && _deferredDefragCopies.empty();
    }

    void CommitStep::swap(CommitStep& other)
    {
        _deferredCopies.swap(other._deferredCopies);
        _deferredDefragCopies.swap(other._deferredDefragCopies);
    }

    CommitStep::CommitStep()
    {
    }

    CommitStep::CommitStep(CommitStep&& moveFrom)
    : _deferredCopies(std::move(moveFrom._deferredCopies))
    , _deferredDefragCopies(std::move(moveFrom._deferredDefragCopies))
    {

    }

    CommitStep& CommitStep::operator=(CommitStep&& moveFrom)
    {
        _deferredCopies = std::move(moveFrom._deferredCopies);
        _deferredDefragCopies = std::move(moveFrom._deferredDefragCopies);
        return *this;
    }

    CommitStep::~CommitStep()
    {
    }

}
