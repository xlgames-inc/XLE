// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TechniqueMaterial.h"

namespace RenderCore { namespace Techniques
{
	Material::Material() { _techniqueConfig[0] = '\0'; }

	Material::Material(Material&& moveFrom) never_throws
	: _bindings(std::move(moveFrom._bindings))
	, _matParams(std::move(moveFrom._matParams))
	, _stateSet(moveFrom._stateSet)
	, _constants(std::move(moveFrom._constants))
	{
		XlCopyString(_techniqueConfig, moveFrom._techniqueConfig);
	}

	Material& Material::operator=(Material&& moveFrom) never_throws
	{
		_bindings = std::move(moveFrom._bindings);
		_matParams = std::move(moveFrom._matParams);
		_stateSet = moveFrom._stateSet;
		_constants = std::move(moveFrom._constants);
		XlCopyString(_techniqueConfig, moveFrom._techniqueConfig);
		return *this;
	}
}}

