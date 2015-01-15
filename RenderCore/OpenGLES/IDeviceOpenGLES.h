// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../IDevice.h"
#include "../../Utility/IntrusivePtr.h"

#define FLEX_USE_VTABLE_DeviceOpenGLES FLEX_USE_VTABLE_Device

typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLContext;
typedef void *EGLSurface;

namespace EGL
{
    typedef EGLDisplay      Display;
    typedef EGLConfig       Config;
    typedef EGLContext      Context;
    typedef EGLSurface      Surface;
}

namespace RenderCore
{
    namespace Metal_OpenGLES { class DeviceContext; }

    ////////////////////////////////////////////////////////////////////////////////

#define FLEX_INTERFACE DeviceOpenGLES
/*-----------------*/ #include "../FlexBegin.h" /*-----------------*/
    
        /// <summary>IDevice extension for OpenGLES</summary>
        /// Use IDevice::QueryInterface for query for this type from a
        /// plain IDevice.
        class /*__declspec( uuid("{01B66C67-7553-4F26-8A21-A4E0756C4738}") )*/ ICLASSNAME(DeviceOpenGLES)
        {
        public:
            IMETHOD EGL::Display                                GetUnderlyingDisplay()  IPURE;
            IMETHOD intrusive_ptr<Metal_OpenGLES::DeviceContext>   CreateDeferredContext() IPURE;
            IMETHOD intrusive_ptr<Metal_OpenGLES::DeviceContext>   GetImmediateContext()   IPURE;
            IDESTRUCTOR
        };

        #if !defined(FLEX_CONTEXT_DeviceOPENGLES)
            #define FLEX_CONTEXT_DeviceOPENGLES     FLEX_CONTEXT_INTERFACE
        #endif

        #if defined(DOXYGEN)
            typedef IDeviceOpenGLES Base_DeviceOpenGLES;
        #endif

/*-----------------*/ #include "../FlexEnd.h" /*-----------------*/

    #if FLEX_CONTEXT_DeviceOpenGLES != FLEX_CONTEXT_CONCRETE
        namespace Metal_OpenGLES { class GlobalResources; }
        Metal_OpenGLES::GlobalResources&        GetGlobalResources();
    #endif

}
