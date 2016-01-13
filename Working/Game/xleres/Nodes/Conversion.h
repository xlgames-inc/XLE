// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(NODES_CONVERSION_H)
#define NODES_CONVERSION_H

#include "../Colour.h"
#include "../gbuffer.h"

float3 ConvertSRGBToLinear(float3 srgbInput)    { return SRGBToLinear(srgbInput); }
float3 ConvertLinearToSRGB(float3 linearInput)  { return LinearToSRGB(linearInput); }

PerPixelMaterialParam MakeMaterialParams(float roughness, float specular, float metal)
{
    PerPixelMaterialParam result = PerPixelMaterialParam_Default();
    result.roughness = roughness;
    result.specular = specular;
    result.metal = metal;
    return result;
}

#endif
