// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

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
    return aveDepthValue;
}
