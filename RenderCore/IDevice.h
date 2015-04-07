// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IDevice_Forward.h"
#include "IThreadContext_Forward.h"
#include "../Core/Prefix.h"
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
////////////////////////////////////////////////////////////////////////////////

    class ViewportContext
    {
    public:
        UInt2 _dimensions;

        ViewportContext() : _dimensions(0,0) {}
        ViewportContext(UInt2 dims) : _dimensions(dims) {}
    };

#define FLEX_INTERFACE PresentationChain
/*-----------------*/ #include "FlexBegin.h" /*-----------------*/
    
        ///
        /// <summary>Represents a set of back buffer for rendering to a window</summary>
        ///
        /// For most platforms we require 1 or more back buffers, and some output
        /// window to render on. This is want the presentation chain is for.
        ///
        /// Normally there is only one RenderCore::Device, but sometimes we need multiple
        /// PresentationChains (for example, if we want to render to multiple windows, in
        /// an editor.
        ///
        /// PresentationChain closely matches IDXGISwapChain behaviour in windows.
        ///
        /// Call RenderCore::IDevice::CreatePresentationChain to create a new chain.
        ///
        /// <seealso cref="RenderCore::IDevice::CreatePresentationChain"/>
        ///
        class ICLASSNAME(PresentationChain)
        {
        public:
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
            /// Normally, present is used like this:
            ///
            ///     <code>\code
            ///     RenderCore::IDevice* device = ...;
            ///     RenderCore::IPresentationChain* presentationChain = ...;
            ///     device->BeginFrame(presentationChain);
            ///         ClearBackBufferAndDepthBuffer(device);   // (helps synchronisation in multi-GPU setups)
            ///         DoRendering(device);
            ///     presentationChain->Present();
            ///     \endcode</code>
            ///
            ///   But in theory we can call Present at any time.
            /// </example>
            IMETHOD void                    Present() IPURE;

            /// <summary>Resizes the presentation chain</summary>
            /// Resizes the presentation chain. Normally this is called after the
            /// output window changes size. If the presentation chain size doesn't
            /// match the output window's size, the behaviour is undefined (though
            /// on windows, the output is stretched to the window size).
            ///
            /// Use the default arguments to automatically adjust to the same size as 
            /// the window.
            IMETHOD void                    Resize(unsigned newWidth=0, unsigned newHeight=0) IPURE;

            /// <summary>Returns a context object that will track the size of the viewport</summary>
            IMETHOD std::shared_ptr<ViewportContext>        GetViewportContext() const IPURE;
            IDESTRUCTOR
        };

        #if !defined(FLEX_CONTEXT_PresentationChain)
            #define FLEX_CONTEXT_PresentationChain     FLEX_CONTEXT_INTERFACE
        #endif

        #if defined(DOXYGEN)
            typedef IPresentationChain Base_PresentationChain;
        #endif

/*-----------------*/ #include "FlexEnd.h" /*-----------------*/

#define FLEX_INTERFACE Device
/*-----------------*/ #include "FlexBegin.h" /*-----------------*/

        ///
        /// <summary>Represents a hardware device capable of rendering</summary>
        ///
        /// IDevice represents a single hardware device that can render. Usually
        /// it is the first rendering object created. Most rendering objects are
        /// associated with a single device (because the device defines the format
        /// and memory location of the object). So a device must be created and
        /// selected before anything else is created.
        ///
        /// Normally there is only a single device. Multiple devices are only
        /// required in very special case situations (for example, if a PC has 2 graphics
        /// cards, and you want to render using both cards).
        ///
        /// Call CreateDevice() to create a new default with the default parameters.
        ///
        /// Normally clients should create a device first, and then create a presentation
        /// chain once an output window has been created.
        ///
        /// To render a frame, call BeginFrame() on the device first, and then call
        /// PresentationChain::Present when the frame is finished.
        ///
        /// You can use "QueryInterface" to get extended interfaces for the device. Some
        /// platforms might expose special case behaviour. To get access, use QueryInterface
        /// to check if the device supports the behaviour you want.
        ///
        /// Note that there is no reference counting behaviour built into IDevice (or any
        /// RenderCore object). But you can use std::shared_ptr<> to get that behaviour. 
        /// If necessary, we can add a CreateDeviceShared() that will use std::make_shared<>
        /// to create an efficient referenced counted object.
        ///
        class ICLASSNAME(Device)
        {
        public:
            /// <summary>Initialised a window for rendering</summary>
            /// To render to a window, we first need to create a presentation chain. This
            /// creates the buffers necessary to render to that window. 
            /// <param name="platformWindowHandle">A platform specific value representing a window. On windows, 
            /// this is would be a HWND value</param>
            /// <param name="width">Width of the presentation chain. RenderCore can't call GetClientRect() on the
            /// window directly, because that would require adding a linker reference to windows dlls. But normally,
            /// with and height are the same size as the window client area. If a different size is used, the behaviour
            /// might be different on different platforms (but on windows, the output is stretched.</param>
            /// <param name="height">see <paramref name="width"/></param>
            IMETHOD std::unique_ptr<IPresentationChain>     CreatePresentationChain(const void* platformWindowHandle, unsigned width, unsigned height) IPURE;

            /// <summary>Looks for compatibility with another interface</summary>
            /// Some implementations of IDevice might provide extension interfaces. 
            /// to extensions. 
            ///
            /// Note that reference counting behaviour is not the same as DirectX/COM QueryInterface.
            /// RenderCore objects don't have reference counting built it. So we can't increase
            /// the reference count on return. So don't delete or deref the returned object.
            /// As a result, be careful that another thread doesn't delete the object as you're using
            /// it.
            ///
            /// <example>
            /// Example:
            ///     <code>\code
            ///     RenderCore::IDeviceDX11* dx11Device = 
            ///          (RenderCore::IDeviceDX11*)device->QueryInterface(__uuidof(RenderCore::IDeviceDX11));
            ///     if (dx11Device) {
            ///         ...
            ///     }
            ///     \endcode</code>
            /// </example>
            ///
            /// <param name="guid">A large random value that is unique to the interface. On the visual studio compiler, use
            /// the __uuidof() extension to get the correct guid for a class. For example, To query for the "IDeviceDX11"
            /// interface, use __uuid(RenderCore::IDeviceDX11).</param>
            /// <returns>Returns nullptr if the interface isn't supported</returns>
            /// <seealso cref="RenderCore::IDeviceDX11"/>
            /// <seealso cref="RenderCore::IDeviceOpenGLES"/>
            IMETHOD virtual void*       QueryInterface(const GUID& guid) IPURE;

            /// <summary>Begins rendering of a new frame</summary>
            /// Starts rendering of a new frame. The frame is ended with a call to RenderCore::IPresentationChain::Present();
            /// You must pass a presentationChain. This defines how the frame will be presented to the user.
            /// Note that rendering to offscreen surfaces can happen outside of the BeginFrame/Present boundaries.
            /// <seealso cref="RenderCore::IPresentationChain::Present"/>
            IMETHOD void                BeginFrame(IPresentationChain* presentationChain) IPURE;

            IMETHOD std::shared_ptr<IThreadContext>     GetImmediateContext() IPURE;
            IMETHOD std::unique_ptr<IThreadContext>     CreateDeferredContext() IPURE;

            /// <summary>Returns version information for this device</summary>
            /// Queries build number and build date information.
            /// The build number is in a format such as:
            /// <code>\code
            ///     vX.Y.Z-[commits]-[commit marker]-[configuration]
            /// \endcode</code>
            /// Here, X, Y, Z are major, minor and patch version.
            /// <list>
            ///     <item> [commits] is the number of extra commits past the version tag in git.
            ///     <item> [commit marker] is the short name of the latest commit to git.
            ///     <item> [configuration] is the build configuration
            /// </list>
            /// The build date format is determined by the OS and locale at compilation time.
            /// <returns>The first returned string is the build number, the second is the build date</returns>
            IMETHOD std::pair<const char*, const char*> GetVersionInformation() IPURE;
            IDESTRUCTOR
        };

        #if !defined(FLEX_CONTEXT_Device)
            #define FLEX_CONTEXT_Device                FLEX_CONTEXT_INTERFACE
        #endif

        #if FLEX_CONTEXT_Device != FLEX_CONTEXT_CONCRETE
            std::shared_ptr<IDevice>    CreateDevice();
        #endif
            
        #if defined(DOXYGEN)
            typedef IDevice Base_Device;
        #endif

/*-----------------*/ #include "FlexEnd.h" /*-----------------*/

////////////////////////////////////////////////////////////////////////////////
}
