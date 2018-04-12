// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Metal/Format.h"
#include <memory>

namespace RenderCore
{
    namespace Metal_OpenGLES { class DeviceContext; }

    ////////////////////////////////////////////////////////////////////////////////

    /// <summary>IDevice extension for OpenGLES</summary>
    /// Use IDevice::QueryInterface for query for this type from a
    /// plain IDevice.
    class IDeviceOpenGLES
    {
    public:
        virtual Metal_OpenGLES::FeatureSet::BitField GetFeatureSet() = 0;
        virtual ~IDeviceOpenGLES();
    };

    class IThreadContextOpenGLES
    {
    public:
        virtual const std::shared_ptr<Metal_OpenGLES::DeviceContext>& GetDeviceContext() = 0;
        virtual ~IThreadContextOpenGLES();
    };

    using Base_DeviceOpenGLES = IDeviceOpenGLES;
    using Base_ThreadContextOpenGLES = IThreadContextOpenGLES;

}
