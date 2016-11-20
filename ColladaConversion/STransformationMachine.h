// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace RenderCore { namespace Assets { namespace GeoProc { class NascentTransformationMachine; class SkeletonRegistry; }}}

namespace ColladaConversion
{
	class Transformation;
    unsigned PushTransformations(
        RenderCore::Assets::GeoProc::NascentTransformationMachine& dst,
        const ::ColladaConversion::Transformation& transformations,
        const char nodeName[],
        const RenderCore::Assets::GeoProc::SkeletonRegistry& nodeRefs, bool assumeEverythingAnimated = false);
}
