// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IBufferUploads.h"
#include <utility>
#include <thread>

namespace BufferUploads
{
        /////////////////////////////////////////////////

    class AssemblyLine;
    class ThreadContext;
    namespace PlatformInterface { class GPUEventStack; }

    class Manager : public IManager
    {
    public:
        void                    UpdateData(TransactionID id, const std::shared_ptr<IDataPacket>& data, const PartialResource&) override;

        TransactionMarker       Transaction_Begin(const ResourceDesc& desc, const std::shared_ptr<IDataPacket>& data, TransactionOptions::BitField flags) override;
        TransactionMarker       Transaction_Begin(const std::shared_ptr<IAsyncDataSource>& data, TransactionOptions::BitField flags) override;
        TransactionMarker       Transaction_Begin(intrusive_ptr<ResourceLocator> & locator, TransactionOptions::BitField flags=0) override;
        void                    Transaction_Cancel(TransactionID id);
        void                    Transaction_Validate(TransactionID id);

        ResourceLocator         Transaction_Immediate(
                                    std::shared_ptr<IThreadContext>& threadContext,
                                    const ResourceDesc& desc, DataPacket& data,
                                    const PartialResource&);
        
        ResourceLocator         GetResource(TransactionID id);
        void                    Resource_Validate(const ResourceLocator& locator);
        bool                    IsCompleted(TransactionID id);

        CommandListMetrics      PopMetrics();
        PoolSystemMetrics       CalculatePoolMetrics() const;
        size_t                  ByteCount(const ResourceDesc& desc) const;

        void                    Update(RenderCore::IThreadContext&, bool preserveRenderState);
        void                    Flush();
        void                    FramePriority_Barrier();

        EventListID             EventList_GetLatestID();
        void                    EventList_Get(EventListID id, Event_ResourceReposition*&begin, Event_ResourceReposition*&end);
        void                    EventList_Release(EventListID id);

        void                    OnLostDevice();
        void                    OnResetDevice();

        Manager(RenderCore::IDevice& renderDevice);
        ~Manager();

    private:
        std::unique_ptr<AssemblyLine> _assemblyLine;
        unsigned _foregroundStepMask, _backgroundStepMask;

        std::unique_ptr<std::thread> _backgroundThread;
        std::unique_ptr<ThreadContext> _backgroundContext;
        std::unique_ptr<ThreadContext> _foregroundContext;
        std::unique_ptr<PlatformInterface::GPUEventStack> _gpuEventStack;

        bool _shutdownBackgroundThread;
        XlHandle _assemblyLineWakeUpEvent, _waitingForDeviceResetEvent;
        bool _handlingLostDevice;

        uint32_t DoBackgroundThread();

        ThreadContext* MainContext();
        const ThreadContext* MainContext() const;
    };
}
