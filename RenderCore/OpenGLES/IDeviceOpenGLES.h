// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../IDevice.h"
#include "../../Utility/IntrusivePtr.h"

#define FLEX_USE_VTABLE_DeviceOpenGLES FLEX_USE_VTABLE_Device
#define FLEX_USE_VTABLE_ThreadContextOpenGLES FLEX_USE_VTABLE_Device

namespace RenderCore
{
    namespace Metal_OpenGLES { class DeviceContext; }

    ////////////////////////////////////////////////////////////////////////////////

#define FLEX_INTERFACE DeviceOpenGLES
/*-----------------*/ #include "../FlexBegin.h" /*-----------------*/

        /// <summary>IDevice extension for OpenGLES</summary>
        /// Use IDevice::QueryInterface for query for this type from a
        /// plain IDevice.
        class ICLASSNAME(DeviceOpenGLES)
        {
        public:
            IMETHOD Metal_OpenGLES::DeviceContext * GetImmediateDeviceContext()   IPURE;
            IDESTRUCTOR
        };

        #if !defined(FLEX_CONTEXT_DeviceOpenGLES)
            #define FLEX_CONTEXT_DeviceOpenGLES     FLEX_CONTEXT_INTERFACE
        #endif

        #if defined(DOXYGEN)
            typedef IDeviceOpenGLES Base_DeviceOpenGLES;
        #endif

/*-----------------*/ #include "../FlexEnd.h" /*-----------------*/

    ////////////////////////////////////////////////////////////////////////////////

#define FLEX_INTERFACE ThreadContextOpenGLES
/*-----------------*/ #include "../FlexBegin.h" /*-----------------*/

        class ICLASSNAME(ThreadContextOpenGLES)
        {
        public:
            IMETHOD std::shared_ptr<Metal_OpenGLES::DeviceContext>&  GetUnderlying() IPURE;
            IDESTRUCTOR
        };

        #if !defined(FLEX_CONTEXT_ThreadContextOpenGLES)
            #define FLEX_CONTEXT_ThreadContextOpenGLES     FLEX_CONTEXT_INTERFACE
        #endif

        #if defined(DOXYGEN)
            typedef IThreadContextOpenGLES Base_ThreadContextOpenGLES;
        #endif

/*-----------------*/ #include "../FlexEnd.h" /*-----------------*/

    ////////////////////////////////////////////////////////////////////////////////

}
