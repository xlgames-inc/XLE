// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "RawGeometry.h"

namespace ColladaConversion
{
    class MeshGeometry;
    class URIResolveContext;

    auto Convert(const MeshGeometry& mesh, const URIResolveContext& pubEles)
        -> RenderCore::ColladaConversion::NascentRawGeometry;
}

