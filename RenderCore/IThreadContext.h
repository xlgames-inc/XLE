// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IThreadContext_Forward.h"
#include "IDevice_Forward.h"
#include "IAnnotator_Forward.h"
#include "../Utility/IteratorUtils.h"
#include <memory>

namespace RenderCore
{
    class ThreadContextStateDesc
    {
    public:
        VectorPattern<unsigned, 2> _viewportDimensions;
        unsigned _frameId;
    };


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
    /// with its own state.
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
    class IThreadContext
    {
    public:
		/// <summary>Begins rendering of a new frame</summary>
		/// Starts rendering of a new frame. The frame is ended with a call to RenderCore::IThreadContext::Present();
		/// You must pass a presentationChain. This defines how the frame will be presented to the user.
		/// Note that rendering to offscreen surfaces can happen outside of the BeginFrame/Present boundaries.
		/// <seealso cref="RenderCore::IThreadContext::Present"/>
		virtual IResourcePtr     BeginFrame(IPresentationChain& presentationChain) = 0;

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
		virtual void			Present(IPresentationChain& presentationChain) = 0;

        /// <summary>Finishes some non-presentation GPU work</summary>
        /// When you want to use the GPU for non-presentation work, like rendering to
        /// an offscreen surface, you don't want to call BeginFrame and Present, but
        /// you do still need a way to tell Metal, and the GPU, when you're done.
        ///
        /// To do this, call CommitHeadless().
        ///
        /// Do not call this method if you're between a BeginFrame and Present. A
        /// presentation frame must be ended with a Present.
        ///
        /// You never need to call both Present and this method; Present already
        /// takes care of committing work and starting the next frame.
        virtual void            CommitHeadless() = 0;

        virtual void*           QueryInterface(size_t guid) = 0;
        virtual bool            IsImmediate() const = 0;
        virtual auto			GetDevice() const -> std::shared_ptr<IDevice> = 0;
		virtual void			InvalidateCachedState() const = 0;

		virtual IAnnotator&		GetAnnotator() = 0;

        virtual ThreadContextStateDesc  GetStateDesc() const = 0;
        virtual ~IThreadContext();
    };

}

