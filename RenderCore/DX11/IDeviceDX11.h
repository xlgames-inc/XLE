// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../IDevice.h"
#include "Metal/DX11.h"

#define FLEX_USE_VTABLE_DeviceDX11 FLEX_USE_VTABLE_Device

namespace RenderCore
{
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
            IMETHOD ID3D::DeviceContext*    GetImmediateContext() IPURE;
            IDESTRUCTOR
        };

        #if !defined(FLEX_CONTEXT_DeviceDX11)
            #define FLEX_CONTEXT_DeviceDX11     FLEX_CONTEXT_INTERFACE
        #endif

        #if defined(DOXYGEN)
            typedef IDeviceDX11 Base_DeviceDX11;
        #endif

        ID3D::Device*        GetDefaultUnderlyingDevice();

/*-----------------*/ #include "../FlexEnd.h" /*-----------------*/


}
