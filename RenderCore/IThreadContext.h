// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IThreadContext_Forward.h"
#include "IDevice_Forward.h"
#include "../Utility/IteratorUtils.h"
#include <memory>

#if OUTPUT_DLL
    #define render_dll_export       dll_export
#else
    #define render_dll_export
#endif

struct _GUID;
typedef struct _GUID GUID;

namespace RenderCore
{
    class ThreadContextStateDesc
    {
    public:
        VectorPattern<unsigned, 2> _viewportDimensions;
        unsigned _frameId;
    };

    class FrameBufferDesc;
    class NamedResources;
    class RenderPassBeginDesc;

    class Resource;
	using ResourcePtr = std::shared_ptr<Resource>;

#define FLEX_INTERFACE ThreadContext
/*-----------------*/ #include "FlexBegin.h" /*-----------------*/

    ///
    /// <summary>Represents the context state of a particular thread while rendering</summary>
    ///
    /// There are 2 types of threadContext objects:
    ///     1. immediate
    ///     2. deferred
    ///
    /// Each thread context is associated with a single CPU thread. As a result, the methods
    /// themselves are not-thread-safe -- because they are only called from a single thread.
    /// We need to store the context state on a thread level, because each thread can be working
    /// with a different state, and each thread wants to assume that other threads won't interfere
    /// with it's own state.
    ///
    /// Each device can have only one immediate context. But this can interact directly
    /// with the GPU, and send commands. The thread that owns the immediate context is the primary
    /// rendering thread.
    ///
    /// Other threads can have a "deferred" context. In this context, GPU commands can be queued up.
    /// But, before the GPU can act upon them, their commands must be submitted to the immediate 
    /// context.
    ///
    /// This object is critical for hiding the metal layer from platform-independent libraries. 
    /// Most objects require access to Metal::DeviceContext to perform rendering operations. But 
    /// DeviceContext is a low-level "Metal" layer object. We need a higher level "RenderCore"
    /// object to wrap it.
    ///
    class ICLASSNAME(ThreadContext)
    {
    public:
		/// <summary>Begins rendering of a new frame</summary>
		/// Starts rendering of a new frame. The frame is ended with a call to RenderCore::IThreadContext::Present();
		/// You must pass a presentationChain. This defines how the frame will be presented to the user.
		/// Note that rendering to offscreen surfaces can happen outside of the BeginFrame/Present boundaries.
		/// <seealso cref="RenderCore::IThreadContext::Present"/>
		IMETHOD ResourcePtr     BeginFrame(IPresentationChain& presentationChain) IPURE;

		/// <summary>Finishes a frame, and presents it to the user</summary>
		/// Present() is used to finish a frame, and present it to the user. 
		/// 
		/// The system will often stall in Present(). This is the most likely place
		/// we need to synchronise with the hardware. So, if the CPU is running fast
		/// and the GPU can't keep up, we'll get a stall in Present().
		/// Normally, this is a good thing, because it means we're GPU bound.
		///
		/// Back buffers get flipped when we Present(). So any new rendering after Present
		/// will go to the next frame.
		///
		/// <example>
		///   Normally, present is used like this:
		///
		///     <code>\code
		///     RenderCore::IDevice* device = ...;
		///     RenderCore::IPresentationChain* presentationChain = ...;
		///     threadContext->BeginFrame(*presentationChain);
		///         ClearBackBufferAndDepthBuffer(device);   // (helps synchronisation in multi-GPU setups)
		///         DoRendering(device);
		///     threadContext->Present(*presentationChain);
		///     \endcode</code>
		///
		///   But in theory we can call Present at any time.
		/// </example>
		IMETHOD void			Present(IPresentationChain& presentationChain) IPURE;

        IMETHOD void            BeginRenderPass(const FrameBufferDesc& fbDesc, NamedResources& namedRes, const RenderPassBeginDesc& beginInfo) IPURE;
        IMETHOD void            NextSubpass() IPURE;
        IMETHOD void            EndRenderPass() IPURE;

        IMETHOD virtual void*   QueryInterface(const GUID& guid) IPURE;
        IMETHOD bool            IsImmediate() const IPURE;
        IMETHOD auto			GetDevice() const -> std::shared_ptr<IDevice> IPURE;
        IMETHOD void            ClearAllBoundTargets() const IPURE;
		IMETHOD void			InvalidateCachedState() const IPURE;

        IMETHOD ThreadContextStateDesc  GetStateDesc() const IPURE;
        IDESTRUCTOR
    };

/*-----------------*/ #include "FlexEnd.h" /*-----------------*/

}

