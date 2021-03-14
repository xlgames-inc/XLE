// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IBufferUploads_Forward.h"
#include "../RenderCore/IDevice_Forward.h"
#include "../RenderCore/ResourceDesc.h"
#include <memory>
#include <future>

#if OUTPUT_DLL
    #define buffer_upload_dll_export       dll_export
#else
    #define buffer_upload_dll_export
#endif

namespace BufferUploads
{
	namespace CPUAccess = RenderCore::CPUAccess;
	namespace GPUAccess = RenderCore::GPUAccess;
	namespace BindFlag = RenderCore::BindFlag;
	namespace AllocationRules = RenderCore::AllocationRules;
	using LinearBufferDesc = RenderCore::LinearBufferDesc;
	using TextureSamples = RenderCore::TextureSamples;
	using TextureDesc = RenderCore::TextureDesc;
    using ResourceDesc = RenderCore::ResourceDesc;

    struct BufferMetrics : public RenderCore::ResourceDesc
    {
    public:
        unsigned        _systemMemorySize;
        unsigned        _videoMemorySize;
        const char*     _pixelFormatName;
    };

        /////////////////////////////////////////////////

    class IDataPacket;
    class IAsyncDataSource;
    class ResourceLocator;

        /////////////////////////////////////////////////

    using TransactionID = uint64_t;
    class Event_ResourceReposition;

    struct BatchedHeapMetrics;
    struct BatchingSystemMetrics;
    struct PoolSystemMetrics;
    struct CommandListMetrics;

        /////////////////////////////////////////////////

    namespace TransactionOptions
    {
        enum {
            LongTerm         = 1<<0,
            FramePriority    = 1<<1
        };
        using BitField = unsigned;
    }

    /// <summary>Specifies a limited part of a resource</summary>
    /// When we want to upload new data for only part of a resource
    /// (for example, just one mip map), we can use PartialResource
    /// to define a limited area within a resource
    class PartialResource
    {
    public:
        RenderCore::Box2D _box;
        unsigned _lodLevelMin = 0, _lodLevelMax = 0;
        unsigned _arrayIndexMin = 0, _arrayIndexMax = 0;
    };

    class TransactionMarker
    {
    public:
        std::future<ResourceLocator> _future;
        TransactionID _transactionID;
    };

        /////////////////////////////////////////////////

    class IManager
    {
    public:
            /// \name Upload Data to an existing transaction
            /// @{

            /// <summary>Use UpdateData to change the data within an existing object</summary>
            /// Upload data for buffer uploads can be provided either to the Transaction_Begin
            /// call, or to UploadData. Use UploadData when you want to update an existing resource,
            /// or change the data that's already present.
        virtual void            UpdateData  (TransactionID id, const std::shared_ptr<IDataPacket>& data, const PartialResource& = PartialResource()) = 0;
            /// @}

            /// \name Begin and End transactions
            /// @{

            /// <summary>Begin a new transaction</summary>
            /// Begin a new transaction, either by creating a new resource, or by attaching
            /// to an existing resource.
        virtual TransactionMarker   Transaction_Begin    (const ResourceDesc& desc, const std::shared_ptr<IDataPacket>& data, TransactionOptions::BitField flags=0) = 0;
        virtual TransactionMarker   Transaction_Begin    (const std::shared_ptr<IAsyncDataSource>& data, TransactionOptions::BitField flags=0) = 0;
        virtual TransactionMarker   Transaction_Begin    (intrusive_ptr<ResourceLocator> & locator, TransactionOptions::BitField flags=0) = 0;

        virtual void            Transaction_Cancel      (TransactionID id) = 0;

            /// <summary>Validates a transaction</summary>
            /// This is a tool for debugging. Checks a transaction for common problems.
            /// Only implemented in _DEBUG builds. Errors will invoke an assert.
        virtual void            Transaction_Validate (TransactionID id) = 0;
            /// @}

            /// \name Immediate creation
            /// @{

            /// <summary>Create a new buffer synchronously</summary>
            /// Creates a new resource synchronously. All creating objects will
            /// execute in the current thread, and a new resource will be returned from
            /// the call. Use these methods when uploads can't be delayed.
        virtual ResourceLocator
            Transaction_Immediate(  std::shared_ptr<IThreadContext>& threadContext,
                                    const ResourceDesc& desc, DataPacket& data,
                                    const PartialResource& = PartialResource()) = 0;
            /// @}

            /// <summary>Checks for completion</summary>
            /// Returns true iff the given transaction has been completed.
        virtual bool            IsCompleted (TransactionID id) = 0;

            /// \name Event queue
            /// @{

        typedef uint32_t EventListID;
        virtual EventListID     EventList_GetLatestID   () = 0;
        virtual void            EventList_Get           (EventListID id, Event_ResourceReposition*&begin, Event_ResourceReposition*&end) = 0;
        virtual void            EventList_Release       (EventListID id) = 0;
            /// @}

            /// \name Resource references
            /// @{
        virtual void            Resource_Validate           (const ResourceLocator& locator) = 0;
            /// @}

            /// \name Frame management
            /// @{

            /// <summary>Called every frame to update uploads</summary>
            /// Performs once-per-frame tasks. Normally called by the render device once per frame.
            /// <param name="preserveRenderState">Set to true to preserve the render state in the 
            ///     immediate context. When set to false, sometimes the render state will be reset
            ///     to the default (See DirectX documentation for ExecuteCommandList)</param>
        virtual void                    Update  (RenderCore::IThreadContext& immediateContext, bool preserveRenderState) = 0;
            /// @}

            /// \name Utilities, profiling & debugging
            /// @{

            /// <summary>Stalls until all work queues are empty</summary>
            /// Normally should only be used during shutdown and loading.
        virtual void                    Flush                   () = 0;
            /// <summary>Gets performance metrics</summary>
            /// Gets the latest performance metrics. Internally the system
            /// maintains a queue of performance metrics. Every frame, a new
            /// set of metrics is pushed onto the queue (until the stack reaches
            /// it's maximum size).
            /// PopMetrics() will remove the next item from the queue. If there
            /// no more items, "_commitTime" will be 0.
        virtual CommandListMetrics      PopMetrics              () = 0;
            /// <summary>Returns the size of a buffer</summary>
            /// Calculates the size of a buffer from a description. This can be
            /// used to estimate the amount of GPU memory that will be used.
        virtual size_t                  ByteCount               (const ResourceDesc& desc) const = 0;
            /// <summary>Returns metrics about pool memory</summary>
            /// Returns some profiling metrics related to the resource pooling
            /// buffers maintained by the system. Used by the BufferUploadDisplay
            /// for presenting profiling information.
        virtual PoolSystemMetrics       CalculatePoolMetrics    () const = 0;
            /// <summary>Sets a barrier for frame priority operations</summary>
            /// Sets a barrier, which determines the "end of frame" point for
            /// frame priority operations. This will normally be called from the same
            /// thread that begins most upload operations.
        virtual void                    FramePriority_Barrier   () = 0;
            /// @}

        virtual ~IManager();
    };

    buffer_upload_dll_export std::unique_ptr<IManager>      CreateManager(RenderCore::IDevice& renderDevice);

}

