// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Math/Vector.h"

namespace RenderCore { namespace Techniques
{
    namespace ObjectCBs
    {
        static const auto LocalTransform = Hash64("LocalTransform");
        static const auto BasicMaterialConstants = Hash64("BasicMaterialConstants");
    }

    class BasicMaterialConstants
    {
    public:
            // fixed set of material parameters currently.
        Float3 _materialDiffuse;    float _opacity;
        Float3 _materialSpecular;   float _alphaThreshold;
    };
}}

