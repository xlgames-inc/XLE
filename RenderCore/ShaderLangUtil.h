// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/ParameterBox.h"
#include "../Utility/StringUtils.h"
#include "../Utility/Optional.h"

namespace RenderCore
{
	enum class ShaderLanguage
	{
		HLSL,
		GLSL,
		MetalShaderLanguage     // (ie, Apple Metal)
	};

	ImpliedTyping::TypeDesc ShaderLangTypeNameAsTypeDesc(StringSection<char> shaderLangTypeName);
	std::string AsShaderLangTypeName(const ImpliedTyping::TypeDesc& type, ShaderLanguage language);

	enum class DescriptorType;
	DescriptorType ShaderLangTypeNameAsDescriptorType(StringSection<char> shaderLangTypeName);
}

