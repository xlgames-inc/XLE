// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../Colour.h"
#include "../../Transform.h"
#include "../../TransformAlgorithm.h"

Texture2DMS<float> DepthsTexture;

float4 ps_depthave(float4 position : SV_Position) : SV_Target
{
    uint2 dims; uint sampleCount;
    DepthsTexture.GetDimensions(dims.x, dims.y, sampleCount);

    float aveDepthValue = 0.f;
    for (uint c=0; c<sampleCount; ++c)
        aveDepthValue += DepthsTexture.Load(uint2(position.xy), c);
    aveDepthValue /= float(sampleCount);
    if (aveDepthValue >= 1.f) discard;
    aveDepthValue = NDCDepthToWorldSpace_Perspective(aveDepthValue, GlobalMiniProjZW());
    return float4(LightingScale * aveDepthValue.xxx / 100.f, 0.f);
}
