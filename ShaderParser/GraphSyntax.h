// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ShaderPatcher.h"
#include "../Utility/StringUtils.h"

namespace Assets { class DirectorySearchRules; }
namespace ShaderPatcher
{
	class GraphSyntaxFile
	{
	public:
		class SubGraph
		{
		public:
			std::string _name;
			NodeGraph _graph;
			NodeGraphSignature _signature;
		};
	};
	std::vector<GraphSyntaxFile::SubGraph> ParseGraphSyntax(StringSection<char> sourceCode);
	std::string ReadGraphSyntax(StringSection<char> input, const ::Assets::DirectorySearchRules& searchRules);
}

