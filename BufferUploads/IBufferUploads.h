// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IBufferUploads_Forward.h"

#include "../RenderCore/IDevice_Forward.h"
#include "../RenderCore/IThreadContext_Forward.h"
#include "../RenderCore/ResourceDesc.h"

#include "../Utility/IntrusivePtr.h"
#include "../Utility/Mixins.h"
#include "../Core/Types.h"
#include <memory>

#if OUTPUT_DLL
    #define buffer_upload_dll_export       dll_export
#else
    #define buffer_upload_dll_export
#endif

namespace ConsoleRig { class GlobalServices; }

namespace BufferUploads
{
	// The "BufferDesc" objects used to be in this namespace, but were moved to
	// the RenderCore namespace. Aliases provided here for backward compatibility.
	namespace CPUAccess = RenderCore::CPUAccess;
	namespace GPUAccess = RenderCore::GPUAccess;
	namespace BindFlag = RenderCore::BindFlag;
	namespace AllocationRules = RenderCore::AllocationRules;
	using LinearBufferDesc = RenderCore::LinearBufferDesc;
	using TextureSamples = RenderCore::TextureSamples;
	using TextureDesc = RenderCore::TextureDesc;
	using BufferDesc = RenderCore::ResourceDesc;

    struct BufferMetrics : public BufferDesc
    {
    public:
        unsigned        _systemMemorySize;
        unsigned        _videoMemorySize;
        const char*     _pixelFormatName;
    };

        /////////////////////////////////////////////////

    class DataPacket;
    class ResourceLocator;

        /////////////////////////////////////////////////

    typedef uint64 TransactionID;
    class Event_ResourceReposition;

    struct BatchedHeapMetrics;
    struct BatchingSystemMetrics;
    struct PoolSystemMetrics;
    struct CommandListMetrics;

    class Box2D
    {
    public:
        signed _left, _top, _right, _bottom;
        Box2D() : _left(0), _top(0), _right(0), _bottom(0) {}
        Box2D(signed left, signed top, signed right, signed bottom) : _left(left), _top(top), _right(right), _bottom(bottom) {}
    };

    bool operator==(const Box2D& lhs, const Box2D& rhs);

        /////////////////////////////////////////////////

    namespace TransactionOptions
    {
        enum {
            LongTerm         = 1<<0,
            FramePriority    = 1<<1,
            ForceCreate      = 1<<2
        };
        typedef unsigned BitField;
    }

    /// <summary>Specifies a limited part of a resource</summary>
    /// When we want to upload new data for only part of a resource
    /// (for example, just one mip map), we can use PartialResource
    /// to define a limited area within a resource
    class PartialResource
    {
    public:
        Box2D _box;
        unsigned _lodLevelMin, _lodLevelMax;
        unsigned _arrayIndexMin, _arrayIndexMax;
        
        PartialResource(const Box2D& box = Box2D(), 
                        unsigned lodLevelMin = 0, unsigned lodLevelMax = 0,
                        unsigned arrayIndexMin = 0, unsigned arrayIndexMax = 0)
        : _box(box), _lodLevelMin(lodLevelMin), _lodLevelMax(lodLevelMax)
        , _arrayIndexMin(arrayIndexMin), _arrayIndexMax(arrayIndexMax) {}
    };

        /////////////////////////////////////////////////

#define FLEX_INTERFACE Manager
/*-----------------*/ #include "../RenderCore/FlexBegin.h" /*-----------------*/

        /// <summary>Main interface for BufferUploads</summary>
        /// BufferUploads::IManager is used as the main interface for uploading
        /// data to the GPU.
        /// Normal usage involves creating a transaction, waiting for the transaction
        /// to complete, and then ending the transaction.
        ///
        /// Use BufferUploads::CreateManager() to create a new manager object.
        ///
        /// Buffer uploads can be used from a separate dll, or statically linked in.
        ///
        /// <example>
        ///     Typical usage:
        ///     <code>\code
        ///         BufferUploads::IManager& manager = ...;
        ///         BufferUploads::BufferDesc desc = ...;
        ///         intrusive_ptr<DataPacket> pkt = ...;
        ///         auto uploadTransaction = manager.Transaction_Begin(desc, initialisationData);
        ///
        ///             // later....
        ///         if (manager.IsCompleted(uploadTransaction)) {
        ///             _myResource = manager.GetResource(uploadTransaction);
        ///             manager.Transaction_End(uploadTransaction);
        ///         }
        ///     \endcode</code>
        /// </example>
    class ICLASSNAME(Manager) : noncopyable
    {
    public:
            /// \name Upload Data to an existing transaction
            /// @{

            /// <summary>Use UpdateData to change the data within an existing object</summary>
            /// Upload data for buffer uploads can be provided either to the Transaction_Begin
            /// call, or to UploadData. Use UploadData when you want to update an existing resource,
            /// or change the data that's already present.
        IMETHOD void            UpdateData  (TransactionID id, DataPacket* rawData, const PartialResource& = PartialResource()) IPURE;
            /// @}

            /// \name Begin and End transactions
            /// @{

            /// <summary>Begin a new transaction</summary>
            /// Begin a new transaction, either by creating a new resource, or by attaching
            /// to an existing resource.
        IMETHOD TransactionID   Transaction_Begin    (const BufferDesc& desc, DataPacket* initialisationData = nullptr, TransactionOptions::BitField flags=0) IPURE;
        IMETHOD TransactionID   Transaction_Begin    (intrusive_ptr<ResourceLocator> & locator, TransactionOptions::BitField flags=0) IPURE;

            /// <summary>Ends a transaction</summary>
            /// Ends a transaction started with Transaction_Begin. Internally, this updates
            /// a reference count. So every call to Transaction_Begin must be balanced with
            /// a call to Transaction_End.
            /// Be sure to end all transactions before destroying the buffer uploads manager.
        IMETHOD void            Transaction_End      (TransactionID id) IPURE;

            /// <summary>Validates a transaction</summary>
            /// This is a tool for debugging. Checks a transaction for common problems.
            /// Only implemented in _DEBUG builds. Errors will invoke an assert.
        IMETHOD void            Transaction_Validate (TransactionID id) IPURE;
            /// @}

            /// \name Immediate creation
            /// @{

            /// <summary>Create a new buffer synchronously</summary>
            /// Creates a new resource synchronously. All creating objects will
            /// execute in the current thread, and a new resource will be returned from
            /// the call. Use these methods when uploads can't be delayed.
        IMETHOD intrusive_ptr<ResourceLocator>
            Transaction_Immediate(  const BufferDesc& desc, DataPacket* initialisationData = nullptr, 
                                    const PartialResource& = PartialResource()) IPURE;
            /// @}

            /// \name Transaction management
            /// @{

            /// <summary>Add extra ref count to transaction</summary>
            /// Adds another reference count to a transaction. Useful when a 
            /// resource is getting cloned.
            /// Should be balanced with a call to Transaction_End
            /// <seealso cref="Transaction_End"/>
        IMETHOD void            AddRef      (TransactionID id) IPURE;

            /// <summary>Checks for completion</summary>
            /// Returns true iff the given transaction has been completed.
        IMETHOD bool            IsCompleted (TransactionID id) IPURE;

            /// <summary>Gets the resource from a completed transaction</summary>
            /// After a transaction has been completed, get the resource with
            /// this method.
            /// Note that the reference count for the returned resource is 
            /// incremented by one in this method. The caller must balance 
            /// that with a call to Resource_Release().
        IMETHOD intrusive_ptr<ResourceLocator>         GetResource (TransactionID id) IPURE;
            /// @}

            /// \name Event queue
            /// @{

        typedef uint32 EventListID;
        IMETHOD EventListID     EventList_GetLatestID   () IPURE;
        IMETHOD void            EventList_Get           (EventListID id, Event_ResourceReposition*&begin, Event_ResourceReposition*&end) IPURE;
        IMETHOD void            EventList_Release       (EventListID id) IPURE;
            /// @}

            /// \name Resource references
            /// @{
        IMETHOD void            Resource_Validate           (const ResourceLocator& locator) IPURE;
            /// <summary>Read back data from a resource</summary>
            /// Read data back from a resource. Sometimes this may require copying data from
            /// the GPU onto the CPU. Note that this can have a significant effect on performance!
            /// If the resource is currently in use by the GPU, it can result in a store.
            /// Whenever possible, it's recommended to avoid using this method. It's provided for
            /// compatibility and debugging.
        IMETHOD intrusive_ptr<DataPacket>  Resource_ReadBack           (const ResourceLocator& locator) IPURE;
            /// @}

            /// \name Frame management
            /// @{

            /// <summary>Called every frame to update uploads</summary>
            /// Performs once-per-frame tasks. Normally called by the render device once per frame.
            /// <param name="preserveRenderState">Set to true to preserve the render state in the 
            ///     immediate context. When set to false, sometimes the render state will be reset
            ///     to the default (See DirectX documentation for ExecuteCommandList)</param>
        IMETHOD void                    Update  (RenderCore::IThreadContext& immediateContext, bool preserveRenderState) IPURE;
            /// @}

            /// \name Utilities, profiling & debugging
            /// @{

            /// <summary>Stalls until all work queues are empty</summary>
            /// Normally should only be used during shutdown and loading.
        IMETHOD void                    Flush                   () IPURE;
            /// <summary>Gets performance metrics</summary>
            /// Gets the latest performance metrics. Internally the system
            /// maintains a queue of performance metrics. Every frame, a new
            /// set of metrics is pushed onto the queue (until the stack reaches
            /// it's maximum size).
            /// PopMetrics() will remove the next item from the queue. If there
            /// no more items, "_commitTime" will be 0.
        IMETHOD CommandListMetrics      PopMetrics              () IPURE;
            /// <summary>Returns the size of a buffer</summary>
            /// Calculates the size of a buffer from a description. This can be
            /// used to estimate the amount of GPU memory that will be used.
        IMETHOD size_t                  ByteCount               (const BufferDesc& desc) const IPURE;
            /// <summary>Returns metrics about pool memory</summary>
            /// Returns some profiling metrics related to the resource pooling
            /// buffers maintained by the system. Used by the BufferUploadDisplay
            /// for presenting profiling information.
        IMETHOD PoolSystemMetrics       CalculatePoolMetrics    () const IPURE;
            /// <summary>Sets a barrier for frame priority operations</summary>
            /// Sets a barrier, which determines the "end of frame" point for
            /// frame priority operations. This will normally be called from the same
            /// thread that begins most upload operations.
        IMETHOD void                    FramePriority_Barrier   () IPURE;
            /// @}

        IDESTRUCTOR
    };

    #if !defined(FLEX_CONTEXT_Manager)
        #define FLEX_CONTEXT_Manager     FLEX_CONTEXT_INTERFACE
    #endif

    #if defined(DOXYGEN)
        typedef IManager Base_Manager;
    #endif

    buffer_upload_dll_export std::unique_ptr<IManager>      CreateManager(RenderCore::IDevice* renderDevice);

    buffer_upload_dll_export void AttachLibrary(ConsoleRig::GlobalServices&);
    buffer_upload_dll_export void DetachLibrary();

/*-----------------*/ #include "../RenderCore/FlexEnd.h" /*-----------------*/

}

