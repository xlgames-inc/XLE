// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Math/Matrix.h"

namespace RenderCore { namespace Assets { namespace GeoProc
{
    class NascentRawGeometry;
    class UnboundSkinController;
}}}

namespace ColladaConversion
{
	class ImportConfiguration;
    class MeshGeometry;
    class SkinController;
    class URIResolveContext;

    auto Convert(const MeshGeometry& mesh, const Float4x4& mergedTransform, const URIResolveContext& pubEles, const ImportConfiguration& cfg)
        -> RenderCore::Assets::GeoProc::NascentRawGeometry;

    auto Convert(const SkinController& controller, const URIResolveContext& pubEles, const ImportConfiguration& cfg)
        -> RenderCore::Assets::GeoProc::UnboundSkinController;
}

