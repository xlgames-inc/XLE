// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../Math/Vector.h"

namespace SceneEngine
{
    class LightingParserContext;
    void DrawBasisAxes(RenderCore::Metal::DeviceContext* context, const LightingParserContext& parserContext, const Float3& offset = Float3(0,0,0));
}

