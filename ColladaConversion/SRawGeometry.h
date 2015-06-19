// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace RenderCore { namespace ColladaConversion
{
    class NascentRawGeometry;
    class UnboundSkinController;
    class ImportConfiguration;
}}

namespace ColladaConversion
{
    class MeshGeometry;
    class SkinController;
    class URIResolveContext;

    auto Convert(const MeshGeometry& mesh, const URIResolveContext& pubEles, const RenderCore::ColladaConversion::ImportConfiguration& cfg)
        -> RenderCore::ColladaConversion::NascentRawGeometry;

    auto Convert(const SkinController& controller, const URIResolveContext& pubEles, const RenderCore::ColladaConversion::ImportConfiguration& cfg)
        -> RenderCore::ColladaConversion::UnboundSkinController;
}

