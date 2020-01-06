// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/MaterialScaffold.h"
#include "../Core/Prefix.h"
#include <memory>
#include <assert.h>

namespace RenderCore { namespace Techniques
{
	DEPRECATED_ATTRIBUTE class DrawableMaterial
	{
	public:
		RenderCore::Assets::MaterialScaffoldMaterial _material;
	};
}}

