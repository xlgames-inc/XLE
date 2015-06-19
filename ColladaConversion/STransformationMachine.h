// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace RenderCore { namespace Assets { class NascentTransformationMachine; }}
namespace ColladaConversion { class Transformation; }

namespace RenderCore { namespace ColladaConversion
{
    class SkeletonRegistry;
    unsigned PushTransformations(
        RenderCore::Assets::NascentTransformationMachine& dst,
        const ::ColladaConversion::Transformation& transformations,
        const char nodeName[],
        const SkeletonRegistry& nodeRefs, bool assumeEverythingAnimated = false);
}}
