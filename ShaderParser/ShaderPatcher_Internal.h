// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <string>
#include <tuple>

namespace ShaderSourceParser { class FunctionSignature; class ParameterStructSignature; }
namespace Assets { class DirectorySearchRules; }

namespace ShaderPatcher
{
	std::tuple<std::string, std::string> SplitArchiveName(const std::string& archiveName);
	const ShaderSourceParser::FunctionSignature& LoadFunctionSignature(const std::tuple<std::string, std::string>& splitName, const ::Assets::DirectorySearchRules& searchRules);
    const ShaderSourceParser::ParameterStructSignature& LoadParameterStructSignature(const std::tuple<std::string, std::string>& splitName, const ::Assets::DirectorySearchRules& searchRules);

	// type traits
	bool IsStructType(StringSection<char> typeName);
	bool CanBeStoredInCBuffer(const StringSection<char> type);
}

