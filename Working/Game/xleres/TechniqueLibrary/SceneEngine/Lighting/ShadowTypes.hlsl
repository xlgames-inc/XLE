// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(SHADOW_TYPES_H)
#define SHADOW_TYPES_H

#include "ResolverInterface.hlsl"
#include "ShadowsResolve.hlsl"
#include "CascadeResolve.hlsl"

///////////////////////////////////////////////////////////////////////////////////////////////////
    //   I N T E R F A C E
///////////////////////////////////////////////////////////////////////////////////////////////////

class ShadowResolver_PoissonDisc : IShadowResolver
{
    float Resolve(CascadeAddress cascadeAddress, LightScreenDest screenDesc)
    {
        ShadowResolveConfig config = ShadowResolveConfig_Default();
        config._pcUsePoissonDiskMethod = true;
        return ResolveShadows_Cascade(
            cascadeAddress.cascadeIndex, cascadeAddress.frustumCoordinates, cascadeAddress.miniProjection,
            screenDesc.pixelCoords, screenDesc.sampleIndex,
            config);
    }
};

class ShadowResolver_Smooth : IShadowResolver
{
    float Resolve(CascadeAddress cascadeAddress, LightScreenDest screenDesc)
    {
        ShadowResolveConfig config = ShadowResolveConfig_Default();
        config._pcUsePoissonDiskMethod = false;
        return ResolveShadows_Cascade(
            cascadeAddress.cascadeIndex, cascadeAddress.frustumCoordinates, cascadeAddress.miniProjection,
            screenDesc.pixelCoords, screenDesc.sampleIndex,
            config);
    }
};

class ShadowResolver_CubeMap : IShadowResolver
{
    float Resolve(CascadeAddress cascadeAddress, LightScreenDest screenDesc)
    {
        ShadowResolveConfig config = ShadowResolveConfig_Default();
        return ResolveShadows_CubeMap(
            cascadeAddress.frustumCoordinates.xyz, cascadeAddress.miniProjection,
            screenDesc.pixelCoords, screenDesc.sampleIndex,
            config);
    }
};

class ShadowResolver_None : IShadowResolver
{
    float Resolve(CascadeAddress cascadeAddress, LightScreenDest screenDesc)
    {
        return 1.f;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

    // note, we can use 2 modes for calculating the shadow cascade:
    //	by world position:
    //		calculates the world space position of the current pixel,
    //		and then transforms that world space position into the
    //		shadow cascade coordinates
    //	by camera position:
    //		this transforms directly from the NDC coordinates of the
    //		current pixel into the camera frustum space.
    //
    //	In theory, by camera position might be a little more accurate,
    //	because it skips the world position stage. So the camera to
    //  shadow method has been optimised for accuracy.
static const bool ResolveCascadeByWorldPosition = false;

class CascadeResolver_Orthogonal : ICascadeResolver
{
    CascadeAddress Resolve(float3 worldPosition, float2 camXY, float worldSpaceDepth)
    {
        if (ResolveCascadeByWorldPosition == true) {
            return ResolveCascade_FromWorldPosition(worldPosition, SHADOW_CASCADE_MODE_ORTHOGONAL, false);
        } else {
            return ResolveCascade_CameraToShadowMethod(camXY, worldSpaceDepth, SHADOW_CASCADE_MODE_ORTHOGONAL, false);
        }
    }
};

class CascadeResolver_OrthogonalWithNear : ICascadeResolver
{
    CascadeAddress Resolve(float3 worldPosition, float2 camXY, float worldSpaceDepth)
    {
        if (ResolveCascadeByWorldPosition == true) {
            return ResolveCascade_FromWorldPosition(worldPosition, SHADOW_CASCADE_MODE_ORTHOGONAL, true);
        } else {
            return ResolveCascade_CameraToShadowMethod(camXY, worldSpaceDepth, SHADOW_CASCADE_MODE_ORTHOGONAL, true);
        }
    }
};

class CascadeResolver_Arbitrary : ICascadeResolver
{
    CascadeAddress Resolve(float3 worldPosition, float2 camXY, float worldSpaceDepth)
    {
        if (ResolveCascadeByWorldPosition == true) {
            return ResolveCascade_FromWorldPosition(worldPosition, SHADOW_CASCADE_MODE_ARBITRARY, false);
        } else {
            return ResolveCascade_CameraToShadowMethod(camXY, worldSpaceDepth, SHADOW_CASCADE_MODE_ARBITRARY, false);
        }
    }
};

class CascadeResolver_CubeMap : ICascadeResolver
{
    CascadeAddress Resolve(float3 worldPosition, float2 camXY, float worldSpaceDepth)
    {
        float3 lightPosition = float3(0,1,0);
        return CascadeAddress_CubeMap(worldPosition-lightPosition);
    }
};

class CascadeResolver_None : ICascadeResolver
{
    CascadeAddress Resolve(float3 worldPosition, float2 camXY, float worldSpaceDepth)
    {
        return CascadeAddress_Invalid();
    }
};

#endif
