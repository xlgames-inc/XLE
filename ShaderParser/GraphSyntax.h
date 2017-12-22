// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ShaderPatcher.h"
#include "../Utility/StringUtils.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace Assets { class DirectorySearchRules; }
namespace ShaderPatcher
{
	class GraphSyntaxFile
	{
	public:
		class SubGraph
		{
		public:
			std::string 		_name;
			NodeGraphSignature 	_signature;
			NodeGraph 			_graph;
		};

		std::vector<SubGraph> _subGraphs;
		std::unordered_map<std::string, std::string> _imports;
	};
	GraphSyntaxFile ParseGraphSyntax(StringSection<char> sourceCode);
	std::string 	ReadGraphSyntax(StringSection<char> input, const ::Assets::DirectorySearchRules& searchRules);
}

