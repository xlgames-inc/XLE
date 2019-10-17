// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Format.h"
#include "../../../Utility/ParameterBox.h"
#include "IncludeAppleMetal.h"

namespace RenderCore { namespace Metal_AppleMetal
{
    namespace FeatureSet
    {
        enum Flags
        {
            SamplerComparisonFn     = (1<<0),
        };
        using BitField = unsigned;
    };

    MTLPixelFormat AsMTLPixelFormat(RenderCore::Format fmt);
    RenderCore::Format AsRenderCoreFormat(MTLPixelFormat fmt);

    Utility::ImpliedTyping::TypeDesc AsTypeDesc(MTLDataType fmt);
}}
