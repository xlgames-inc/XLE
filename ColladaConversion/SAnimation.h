// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Assets/RawAnimationCurve.h"
#include "../RenderCore/Assets/TransformationCommands.h"
#include "../Core/Exceptions.h"
#include <string>
#include <vector>

namespace ColladaConversion
{
	class Animation; class URIResolveContext;

    class UnboundAnimation
    {
    public:
        class Curve
        {
        public:
            std::string                 _parameterName;
            RenderCore::Assets::RawAnimationCurve   _curve;
			RenderCore::Assets::AnimSamplerType _samplerType;
            unsigned                    _samplerOffset;
        };

        std::vector<Curve> _curves;
    };

    UnboundAnimation Convert(
        const ::ColladaConversion::Animation& animation,
        const ::ColladaConversion::URIResolveContext& resolveContext);
}

