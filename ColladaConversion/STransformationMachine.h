// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/GeoProc/SkeletonRegistry.h"
#include "../Utility/StringUtils.h"
#include <functional>

namespace RenderCore { namespace Assets { namespace GeoProc { class NascentSkeletonMachine; class NascentSkeletonInterface; class SkeletonRegistry; }}}
namespace RenderCore { namespace Assets { class TransformationParameterSet; }}

namespace ColladaConversion
{
	class Transformation;
    unsigned PushTransformations(
        RenderCore::Assets::GeoProc::NascentSkeletonMachine& dst,
		RenderCore::Assets::GeoProc::NascentSkeletonInterface& interf,
		RenderCore::Assets::TransformationParameterSet& defaultParameters,
		const ::ColladaConversion::Transformation& transformations,
        const char nodeName[],
		const std::function<bool(StringSection<>)>& predicate);
        
	// const RenderCore::Assets::GeoProc::SkeletonRegistry& nodeRefs, bool assumeEverythingAnimated = false);
}
