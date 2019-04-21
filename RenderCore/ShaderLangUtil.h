// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/ParameterBox.h"
#include "../Utility/StringUtils.h"

namespace RenderCore
{
    ImpliedTyping::TypeDesc ShaderLangTypeNameAsTypeDesc(StringSection<char> shaderLangTypeName);
	std::string AsShaderLangTypeName(const ImpliedTyping::TypeDesc& typeDesc);
}

