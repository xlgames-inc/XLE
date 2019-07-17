// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IDevice_Forward.h"
#include <memory>

namespace RenderCore
{
    enum class UnderlyingAPI
    {
        DX11, Vulkan, OpenGLES, AppleMetal
    };

    std::shared_ptr<IDevice>    CreateDevice(UnderlyingAPI api);

	using DeviceCreationFunction = std::shared_ptr<IDevice>();
	void RegisterDeviceCreationFunction(
		UnderlyingAPI api,
		DeviceCreationFunction* fn);
}

