// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IBufferUploads.h"

#include "PlatformInterface.h"
#include "Metrics.h"
#include "ResourceLocator.h"
#include "../RenderCore/IThreadContext_Forward.h"
#include "../RenderCore/Resource.h"
#include "../RenderCore/IDevice.h"
#include "../RenderCore/Metal/DeviceContext.h"		// for command list ptr
#include "../Utility/Threading/LockFree.h"

namespace BufferUploads
{
	using UnderlyingResource = RenderCore::Resource;
	using UnderlyingResourcePtr = RenderCore::ResourcePtr;
	using CommandListPtr = RenderCore::Metal::CommandListPtr;

        //////   C O M M I T   S T E P   //////

    class CommitStep
    {
    public:
        class DeferredCopy
        {
        public:
            intrusive_ptr<DataPacket> _temporaryBuffer;
            intrusive_ptr<ResourceLocator> _destination;
            unsigned _size;

            DeferredCopy();
            DeferredCopy(intrusive_ptr<ResourceLocator> destination, unsigned size, intrusive_ptr<DataPacket> pkt);
            DeferredCopy(DeferredCopy&& moveFrom);
            const DeferredCopy& operator=(DeferredCopy&& moveFrom);
            ~DeferredCopy();

        private:
            DeferredCopy(const DeferredCopy& cloneFrom);
            const DeferredCopy& operator=(const DeferredCopy& cloneFrom);
        };

        class DeferredDefragCopy
        {
        public:
            UnderlyingResourcePtr _destination;
            UnderlyingResourcePtr _source;
            std::vector<DefragStep> _steps;
            DeferredDefragCopy(UnderlyingResource* destination, UnderlyingResource* source, const std::vector<DefragStep>& steps);
            ~DeferredDefragCopy();
        };

        void Add(DeferredCopy&& copy);
        void Add(DeferredDefragCopy&& copy);
        void CommitToImmediate_PreCommandList(RenderCore::IThreadContext& immediateContext);
        void CommitToImmediate_PostCommandList(RenderCore::IThreadContext& immediateContext);
        bool IsEmpty() const;

        void swap(CommitStep& other);

        CommitStep();
        CommitStep(CommitStep&& moveFrom);
        CommitStep& operator=(CommitStep&& moveFrom);
        ~CommitStep();
    private:
        std::vector<DeferredCopy>       _deferredCopies;
        std::vector<DeferredDefragCopy> _deferredDefragCopies;

        CommitStep(const CommitStep& copyForm);
        CommitStep& operator=(const CommitStep&);
    };

        //////   C O M M A N D   L I S T   //////

    class CommandList
    {
    public:
        typedef Interlocked::Value ID;

		CommandListPtr					_deviceCommandList;
        mutable CommandListMetrics      _metrics;
        CommitStep                      _commitStep;
        ID                              _id;

        CommandList();
        CommandList(CommandList&& moveFrom);
        CommandList& operator=(CommandList&& moveFrom);
        ~CommandList();
    private:
        CommandList(const CommandList&);
        CommandList& operator=(const CommandList&);
    };

        //////   T H R E A D   C O N T E X T   //////

    #if !defined(XL_RELEASE)
        #define XL_BUFFER_UPLOAD_RECORD_THREAD_CONTEXT_METRICS
    #endif

    class ThreadContext
    {
    public:
        void                    BeginCommandList();
        void                    ResolveCommandList();
        void                    CommitToImmediate(
            RenderCore::IThreadContext& commitTo,
            PlatformInterface::GPUEventStack& gpuEventStack,
            bool preserveRenderState);

        CommandListMetrics      PopMetrics();

        void                    EventList_Get(IManager::EventListID id, Event_ResourceReposition*& begin, Event_ResourceReposition*& end);
        void                    EventList_Release(IManager::EventListID id, bool silent = false);
        IManager::EventListID   EventList_Push(const Event_ResourceReposition& evnt);
        void                    EventList_Publish(IManager::EventListID toEvent);

        IManager::EventListID   EventList_GetWrittenID() const;
        IManager::EventListID   EventList_GetPublishedID() const;
        IManager::EventListID   EventList_GetProcessedID() const;

        CommandList::ID         CommandList_GetUnderConstruction() const        { return _commandListIDUnderConstruction; }
        CommandList::ID         CommandList_GetCompletedByGPU() const           { return _commandListIDCompletedByGPU; }
        CommandList::ID         CommandList_GetCommittedToImmediate() const     { return _commandListIDCommittedToImmediate; }

        PlatformInterface::UnderlyingDeviceContext& GetDeviceContext()          { return _deviceContext; }
        CommandListMetrics&     GetMetricsUnderConstruction()                   { return _commandListUnderConstruction; }
        CommitStep&             GetCommitStepUnderConstruction()                { return _commitStepUnderConstruction; }

        unsigned                CommitCount_Current()                           { return _commitCountCurrent; }
        unsigned&               CommitCount_LastResolve()                       { return _commitCountLastResolve; }

        XlHandle                GetWakeupEvent()                                { return _wakeupEvent; }
        void                    FramePriority_Barrier(unsigned queueSetId);

        void                    OnLostDevice();

        LockFree::FixedSizeQueue<unsigned, 4> _pendingFramePriority_CommandLists;

        ThreadContext(std::shared_ptr<RenderCore::IThreadContext> underlyingContext);
        ~ThreadContext();
    private:
        CommandListMetrics _commandListUnderConstruction;
        CommitStep _commitStepUnderConstruction;
        LockFree::FixedSizeQueue<CommandList, 32> _queuedCommandLists;
        #if defined(XL_BUFFER_UPLOAD_RECORD_THREAD_CONTEXT_METRICS)
            LockFree::FixedSizeQueue<CommandListMetrics, 32> _recentRetirements;
        #endif
        PlatformInterface::UnderlyingDeviceContext _deviceContext;
        std::shared_ptr<RenderCore::IThreadContext> _underlyingContext;

        TimeMarker  _lastResolve;
        TimeMarker  _tickFrequency;
        unsigned    _commitCountCurrent, _commitCountLastResolve;
        bool        _requiresResolves;

        XlHandle    _wakeupEvent;

        CommandList::ID _commandListIDUnderConstruction, _commandListIDCompletedByGPU, _commandListIDCommittedToImmediate;

        class EventList
        {
        public:
            volatile IManager::EventListID _id;
            Event_ResourceReposition _evnt;
            Interlocked::Value _clientReferences;
            EventList() : _id(~IManager::EventListID(0x0)), _clientReferences(0) {}
        };
        IManager::EventListID   _currentEventListId;
        IManager::EventListID   _currentEventListPublishedId;
        Interlocked::Value      _currentEventListProcessedId;
        EventList               _eventBuffers[4];
        unsigned                _eventListWritingIndex;
    };
}
