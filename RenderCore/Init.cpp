// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Init.h"
#include "../ConsoleRig/Log.h"
#include "../Core/Exceptions.h"
#include <vector>
#include <algorithm>
#include <sstream>

namespace RenderCore
{
	static std::vector<std::pair<UnderlyingAPI, DeviceCreationFunction*>>& GetRegisterCreationFunctions()
	{
		static std::vector<std::pair<UnderlyingAPI, DeviceCreationFunction*>> creationFunctions;
		return creationFunctions;
	}

	const char* AsString(UnderlyingAPI api)
	{
		switch (api) {
		case UnderlyingAPI::DX11: return "DX11"; break;
		case UnderlyingAPI::Vulkan: return "Vulkan"; break;
		case UnderlyingAPI::OpenGLES: return "OpenGLES"; break;
		case UnderlyingAPI::AppleMetal: return "AppleMetal"; break;
		}
		return "<<unknown>>";
	}

    std::shared_ptr<IDevice>    CreateDevice(UnderlyingAPI api)
    {
		auto& creationFunctions = GetRegisterCreationFunctions();
		auto i = std::find_if(
			creationFunctions.begin(), creationFunctions.end(),
			[api](const std::pair<UnderlyingAPI, DeviceCreationFunction*>& p) {
				return p.first == api;
			});
		if (i != creationFunctions.end())
			return (*i->second)();

		std::stringstream str;
		str << "No device creation function registered for the given device API. Returning nullptr. These devices are supported:" << std::endl;
		for (const auto& c:creationFunctions)
			str << AsString(c.first) << std::endl;
		Throw(std::runtime_error(str.str().c_str()));
    }

	void RegisterDeviceCreationFunction(
		UnderlyingAPI api,
		DeviceCreationFunction* fn)
	{
		auto& creationFunctions = GetRegisterCreationFunctions();
		auto i = std::find_if(
			creationFunctions.begin(), creationFunctions.end(),
			[api](const std::pair<UnderlyingAPI, DeviceCreationFunction*>& p) {
				return p.first == api;
			});
		if (i != creationFunctions.end()) {
			Log(Warning) << "Multiple device creation function for the same API" << std::endl;
			return;
		}
		creationFunctions.push_back({api, fn});
	}
}

