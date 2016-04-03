// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IThreadContext_Forward.h"
#include "IDevice_Forward.h"
#include "../Math/Vector.h"
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
        Int2 _viewportDimensions;
        unsigned _frameId;
    };

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
        IMETHOD virtual void*   QueryInterface(const GUID& guid) IPURE;
        IMETHOD bool            IsImmediate() const IPURE;
        IMETHOD std::shared_ptr<IDevice>    GetDevice() const IPURE;
        IMETHOD void            ClearAllBoundTargets() const IPURE;
		IMETHOD void			InvalidateCachedState() const IPURE;

        IMETHOD ThreadContextStateDesc  GetStateDesc() const IPURE;
        IDESTRUCTOR
    };

/*-----------------*/ #include "FlexEnd.h" /*-----------------*/

}

