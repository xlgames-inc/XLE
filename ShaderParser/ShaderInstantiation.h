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

	class InstantiatedShader
	{
	public:
		std::vector<std::string> _sourceFragments;
		NodeGraphSignature _entryPointSignature;
	};

	InstantiatedShader InstantiateShader(
		StringSection<> entryFile,
		StringSection<> entryFn,
		const InstantiationParameters& instantiationParameters);

	InstantiatedShader InstantiateShader(
		const INodeGraphProvider::NodeGraph& initialGraph,
		const InstantiationParameters& instantiationParameters);

	InstantiatedShader InstantiateShader(
		StringSection<> shaderName,
		const NodeGraph& graph,
		const std::shared_ptr<INodeGraphProvider>& subProvider,
		const InstantiationParameters& instantiationParameters);
}
