// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace RenderCore { namespace Assets { class RawMaterial; }}

namespace ColladaConversion 
{
    class Effect;
    class URIResolveContext;
}

namespace RenderCore { namespace ColladaConversion
{
    RenderCore::Assets::RawMaterial Convert(
        const ::ColladaConversion::Effect& effect, 
        const ::ColladaConversion::URIResolveContext& pubEles);
}}
