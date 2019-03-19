// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Math/Vector.h"

namespace RenderCore { class IThreadContext; }
namespace RenderCore { namespace Techniques { class ParsingContext; }}

namespace SceneEngine
{
    void DrawBasisAxes(RenderCore::IThreadContext& context, RenderCore::Techniques::ParsingContext& parserContext, const Float3& offset = Float3(0,0,0));
}

