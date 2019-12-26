// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

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
}}
