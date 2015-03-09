// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#define FLEX_CONTEXT_Manager             FLEX_CONTEXT_CONCRETE

#include "IBufferUploads.h"
#include "../Utility/Threading/ThreadObject.h"
#include "../Core/Prefix.h"

#include <utility>

namespace BufferUploads
{
        /////////////////////////////////////////////////

    class AssemblyLine;
    class ThreadContext;
    namespace PlatformInterface { class GPUEventStack; }

    class Manager : public Base_Manager
    {
    public:
        void                    UpdateData(TransactionID id, RawDataPacket* rawData, const PartialResource&);

        TransactionID           Transaction_Begin(const BufferDesc& desc, RawDataPacket* initialisationData = NULL, TransactionOptions::BitField flags=0);
        TransactionID           Transaction_Begin(intrusive_ptr<ResourceLocator>& locator, TransactionOptions::BitField flags=0);
        void                    Transaction_End(TransactionID id);
        void                    Transaction_Validate(TransactionID id);

        intrusive_ptr<ResourceLocator>         Transaction_Immediate(
                                        const BufferDesc& desc, RawDataPacket* initialisationData, 
                                        const PartialResource&);
        
        intrusive_ptr<ResourceLocator>         GetResource(TransactionID id);
        void                    Resource_Validate(const ResourceLocator& locator);
        intrusive_ptr<RawDataPacket>  Resource_ReadBack(const ResourceLocator& locator);
        void                    AddRef(TransactionID id);
        bool                    IsCompleted(TransactionID id);

        CommandListMetrics      PopMetrics();
        PoolSystemMetrics       CalculatePoolMetrics() const;
        size_t                  ByteCount(const BufferDesc& desc) const;

        void                    Update(RenderCore::IThreadContext&);
        void                    Flush();
        void                    FramePriority_Barrier();

        EventListID             EventList_GetLatestID();
        void                    EventList_Get(EventListID id, Event_ResourceReposition*&begin, Event_ResourceReposition*&end);
        void                    EventList_Release(EventListID id);

        void                    OnLostDevice();
        void                    OnResetDevice();

        Manager(RenderCore::IDevice* renderDevice);
        ~Manager();

    private:
        std::unique_ptr<AssemblyLine> _assemblyLine;
        unsigned _foregroundStepMask, _backgroundStepMask;

        std::unique_ptr<Threading::Thread> _backgroundThread;
        std::unique_ptr<ThreadContext> _backgroundContext;
        std::unique_ptr<ThreadContext> _foregroundContext;
        std::unique_ptr<PlatformInterface::GPUEventStack> _gpuEventStack;

        bool _shutdownBackgroundThread;
        XlHandle _assemblyLineWakeUpEvent, _waitingForDeviceResetEvent;
        bool _handlingLostDevice;

        static uint32 xl_thread_call BackgroundThreadFunction(void *);
        uint32 DoBackgroundThread();

        ThreadContext* MainContext();
        const ThreadContext* MainContext() const;
    };
}
