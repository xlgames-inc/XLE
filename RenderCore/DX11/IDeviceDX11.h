// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../IDevice.h"
#include "../IThreadContext.h"
#include "Metal/DX11.h"

#define FLEX_USE_VTABLE_DeviceDX11 FLEX_USE_VTABLE_Device
#define FLEX_USE_VTABLE_ThreadContextDX11 FLEX_USE_VTABLE_ThreadContext

namespace RenderCore
{
    namespace Metal_DX11 { class DeviceContext; }

    ////////////////////////////////////////////////////////////////////////////////

#define FLEX_INTERFACE DeviceDX11
/*-----------------*/ #include "../FlexBegin.h" /*-----------------*/
    
        /// <summary>IDevice extension for DX11</summary>
        /// Use IDevice::QueryInterface to query for this type from a
        /// plain IDevice.
        class __declspec( uuid("{01B66C67-7553-4F26-8A21-A4E0756C4738}") ) ICLASSNAME(DeviceDX11)
        {
        public:
            IMETHOD ID3D::Device*           GetUnderlyingDevice() IPURE;
            IMETHOD ID3D::DeviceContext*    GetImmediateDeviceContext() IPURE;
            IDESTRUCTOR
        };

        #if !defined(FLEX_CONTEXT_DeviceDX11)
            #define FLEX_CONTEXT_DeviceDX11     FLEX_CONTEXT_INTERFACE
        #endif

        #if defined(DOXYGEN)
            typedef IDeviceDX11 Base_DeviceDX11;
        #endif

/*-----------------*/ #include "../FlexEnd.h" /*-----------------*/


    ////////////////////////////////////////////////////////////////////////////////

#define FLEX_INTERFACE ThreadContextDX11
/*-----------------*/ #include "../FlexBegin.h" /*-----------------*/
    
        /// <summary>IThreadContext extension for DX11</summary>
        class __declspec( uuid("{B1985A80-4D9F-4D5B-88CF-657C9F9A6B66}") ) ICLASSNAME(ThreadContextDX11)
        {
        public:
            IMETHOD std::shared_ptr<Metal_DX11::DeviceContext>&  GetUnderlying() IPURE;
            IDESTRUCTOR
        };

        #if !defined(FLEX_CONTEXT_ThreadContextDX11)
            #define FLEX_CONTEXT_ThreadContextDX11     FLEX_CONTEXT_INTERFACE
        #endif

        #if defined(DOXYGEN)
            typedef IThreadContextDX11 Base_ThreadContextDX11;
        #endif

/*-----------------*/ #include "../FlexEnd.h" /*-----------------*/

}
