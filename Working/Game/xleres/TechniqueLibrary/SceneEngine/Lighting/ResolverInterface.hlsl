// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(RESOLVER_INTERFACE_H)
#define RESOLVER_INTERFACE_H

#include "../../Core/gbuffer.hlsl"
#include "LightDesc.hlsl"

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

///////////////////////////////////////////////////////////////////////////////////////////////////
#include "LightShapes.hlsl"

class Directional : ILightResolver
{
    float3 Resolve(
        GBufferValues sample,
        LightSampleExtra sampleExtra,
        LightDesc light,
        float3 worldPosition,
        float3 directionToEye,
        LightScreenDest screenDest)
    {
        return Resolve_Directional(sample, sampleExtra, light, worldPosition, directionToEye, screenDest);
    }
};

class Sphere : ILightResolver
{
    float3 Resolve(
        GBufferValues sample,
        LightSampleExtra sampleExtra,
        LightDesc light,
        float3 worldPosition,
        float3 directionToEye,
        LightScreenDest screenDest)
    {
        return Resolve_Sphere(sample, sampleExtra, light, worldPosition, directionToEye, screenDest);
    }
};

class Tube : ILightResolver
{
    float3 Resolve(
        GBufferValues sample,
        LightSampleExtra sampleExtra,
        LightDesc light,
        float3 worldPosition,
        float3 directionToEye,
        LightScreenDest screenDest)
    {
        return Resolve_Tube(sample, sampleExtra, light, worldPosition, directionToEye, screenDest);
    }
};

class Rectangle : ILightResolver
{
    float3 Resolve(
        GBufferValues sample,
        LightSampleExtra sampleExtra,
        LightDesc light,
        float3 worldPosition,
        float3 directionToEye,
        LightScreenDest screenDest)
    {
        return Resolve_Rectangle(sample, sampleExtra, light, worldPosition, directionToEye, screenDest);
    }
};

#endif
