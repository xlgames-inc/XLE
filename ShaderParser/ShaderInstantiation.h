// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NodeGraphProvider.h"
#include "../Utility/StringUtils.h"
#include <vector>
#include <string>

namespace ShaderPatcher
{
	class InstantiationParameters;
	std::vector<std::string> InstantiateShader(
		StringSection<> entryFile,
		StringSection<> entryFn,
		const ShaderPatcher::InstantiationParameters& instantiationParameters);

	std::vector<std::string> InstantiateShader(
		const INodeGraphProvider::NodeGraph& initialGraph,
		const ShaderPatcher::InstantiationParameters& instantiationParameters);
}
