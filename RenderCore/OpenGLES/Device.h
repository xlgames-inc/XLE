// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#define FLEX_CONTEXT_Device             FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_DeviceOpenGLES     FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_PresentationChain  FLEX_CONTEXT_CONCRETE

#include "../IDevice.h"
#include "IDeviceOpenGLES.h"
#include "../../Utility/Mixins.h"
#include "../../Utility/IntrusivePtr.h"

namespace RenderCore
{
////////////////////////////////////////////////////////////////////////////////

    class PresentationChain : public Base_PresentationChain
    {
    public:
        void                Present() /*override*/;

        EGL::Surface        GetUnderlyingSurface() { return _surface; }

        PresentationChain(EGL::Display display, EGL::Config config, const void* platformValue);
        ~PresentationChain();
    private:
        EGL::Surface        _surface;
        EGL::Display        _display;
    };

////////////////////////////////////////////////////////////////////////////////

    class Device :  public Base_Device, noncopyable
    {
    public:
        std::unique_ptr<IPresentationChain>     CreatePresentationChain(const void* platformValue) /*override*/;
        virtual void*                           QueryInterface(const GUID& guid);

        void                        BeginFrame(IPresentationChain* presentationChain);

        Device();
        ~Device();

    protected:
        EGL::Display                _display;
        EGL::Config                 _config;
        intrusive_ptr<Metal_OpenGLES::DeviceContext>   _immediateContext;
    };

    class DeviceOpenGLES : public Device, public Base_DeviceOpenGLES
    {
    public:
        intrusive_ptr<Metal_OpenGLES::DeviceContext>   CreateDeferredContext();
        intrusive_ptr<Metal_OpenGLES::DeviceContext>   GetImmediateContext();
        virtual void*                               QueryInterface(const GUID& guid);
        EGL::Display                                GetUnderlyingDisplay() { return _display; }

        DeviceOpenGLES();
        ~DeviceOpenGLES();
    };

////////////////////////////////////////////////////////////////////////////////
}
