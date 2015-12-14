// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(RESOLVER_INTERFACE_H)
#define RESOLVER_INTERFACE_H

#include "../gbuffer.h"
#include "LightDesc.h"

struct LightScreenDest
{
    int2 pixelCoords;
    uint sampleIndex;
};

struct LightSampleExtra
{
    float screenSpaceOcclusion;
};

interface ILightResolver
{
    float3 Resolve(
        GBufferValues sample,
        LightSampleExtra sampleExtra,
        LightDesc light,
        float3 worldPosition,
        float3 directionToEye,
        LightScreenDest screenDest);
};

interface IShadowResolver
{
    float Resolve(CascadeAddress cascadeAddress, LightScreenDest screenDesc);
};

interface ICascadeResolver
{
    CascadeAddress Resolve(float3 worldPosition, float2 camXY, float worldSpaceDepth);
};

#endif
