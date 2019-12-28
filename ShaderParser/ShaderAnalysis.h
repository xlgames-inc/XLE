// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Assets/LocalCompiledShaderSource.h"
#include "../Utility/ParameterBox.h"
#include <unordered_map>

namespace Assets { class DirectorySearchRules; }

namespace ShaderSourceParser
{
	RenderCore::Assets::ISourceCodePreprocessor::SourceCodeWithRemapping ExpandIncludes(
		StringSection<> src,
		const std::string& srcName,
		const ::Assets::DirectorySearchRules& searchRules);

	class ShaderSelectorAnalysis
	{
	public:
		std::unordered_map<std::string, std::string> _selectorRelevance;
	};

	ShaderSelectorAnalysis AnalyzeSelectors(const std::string& sourceCode);

	ParameterBox FilterSelectors(
		const ParameterBox& unfiltered,
		const std::unordered_map<std::string, std::string>& relevance);
}
