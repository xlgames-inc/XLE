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

namespace ColladaConversion { class Animation; class URIResolveContext; }
namespace RenderCore { namespace Assets { namespace GeoProc { class SkeletonRegistry; }}}

namespace RenderCore { namespace ColladaConversion
{
    class UnboundAnimation
    {
    public:
        class Curve
        {
        public:
            std::string                 _parameterName;
            Assets::RawAnimationCurve   _curve;
            Assets::TransformationParameterSet::Type::Enum _samplerType;
            unsigned                    _samplerOffset;

            Curve(
                const std::string parameterName, Assets::RawAnimationCurve&& curve,
                Assets::TransformationParameterSet::Type::Enum samplerType,
                unsigned samplerOffset)
            : _parameterName(parameterName), _curve(std::move(curve))
            , _samplerType(samplerType), _samplerOffset(samplerOffset) {}

            Curve(Curve&& moveFrom) never_throws
            : _curve(std::move(moveFrom._curve))
            , _parameterName(moveFrom._parameterName)
            , _samplerType(moveFrom._samplerType)
            , _samplerOffset(moveFrom._samplerOffset)
            {}

            Curve& operator=(Curve&& moveFrom) never_throws
            {
                _parameterName = moveFrom._parameterName;
                _curve = std::move(moveFrom._curve);
                _samplerType = moveFrom._samplerType;
                _samplerOffset = moveFrom._samplerOffset;
                return *this;
            }
        };

        std::vector<Curve> _curves;
    };

    UnboundAnimation Convert(
        const ::ColladaConversion::Animation& animation,
        const ::ColladaConversion::URIResolveContext& resolveContext,
        RenderCore::Assets::GeoProc::SkeletonRegistry& nodeRefs);
}}

