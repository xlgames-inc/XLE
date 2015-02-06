// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(TRANSFORM_ALGORITHM_H)
#define TRANSFORM_ALGORITHM_H

#include "Transform.h"

// 
//        Depth transformations
//      --=====-===============--
//
//  In shaders, we deal with "depth" information in a variety of forms, and we often
//  need to convert between forms on the fly. This provides some utility functions to
//  do that.
//
//      "NDC depth"
//          This the depth values that get written into the depth buffer
//          It's depth value post perspective divide, after the perspective
//          transform. NDC depths should always be either between 0 and 1
//          or between -1 and 1 (depending on how NDC space is defined).
//
//          Importantly, it's not linear in world space. There is more 
//          precision in NDC depth values near the camera, and little
//          precision in the distance.
//
//      "Linear 0 to 1 depth"
//          This is "linear" depth, meaning that it linear against world
//          space depth values. A difference of "d" between two depth values 
//          in this space will always equal a difference of "Rd" in world
//          space, where R is a constant scalar.
//
//          However, the range is still 0 to 1. In this case, 0 is the camera,
//          not the near clip plane! And 1 is the far clip plane. 
//
//          Note that in NDCz, 0 is the near clip plane. But in "linear 0 to 1"
//          depth, 0 is the camera!
//
//          This is useful if we have frustum corner vectors. If we have 
//          world-space vectors from the camera position to the corners 
//          on the far plane clip, we can scale those by the "linear 0 to 1" 
//          depth to calculate the correct resolved position in world space.
//          (it's also convenient if we want to pre-calculate linear space
//          depth values and store them in a texture with values normalized
//          between 0 and 1)
//
//      "World space depth"
//          This is a depth values that corresponds to the world space units
//          (and is linear). That is, a different of "d" between two depths
//          values corresponds to exactly "d" world space units.
//
//          Actually, this could more accurate be called "view space depth",
//          because we're really calculating a distance in view space. However,
//          view space should always have the same scale as world space. So
//          distances in view space will have the same magnitude in world space.
//
//          For these depth values, 0 still corresponds to the near clip plane.
//
//  There are a number of conversion functions to go back and forth between
//  representations.
//
//  Let's consider perspective projections first -- 
//  For perspective projection, we assume a projection matrix of the form:
//          [ X   0   0   0 ]
//          [ 0   Y   0   0 ]
//          [ 0   0   Z   W ]
//          [ 0   0  -1   0 ]
//
//      Here, "float4(X, Y, Z, W)" is called the "minimal projection." This contains
//      enough information to reconstruct the projection matrix in most cases. Note
//      that there are some cases where we might want to use a more complex projection
//      matrix (perhaps to skew in X and Y). But the great majority of perspective 
//      projection matrices will have this basic form.
//
//      let:
//          Vz = view space "z" component
//          NDCz = NDC "z" component
//          Pz, Pw = most projection z and w components
//          Lz = linear 0 to 1 depth
//
//      NDCz = Pz / Pw
//      Pz = Z * Vz + W
//      Pw = -Vz
//      NDCz = (Z * Vz + W) / -Vz
//           = -Z - W / Vz 
//
//      -W/Vz = NDCz + Z
//      Vz = -W / (NDCz + Z)
//
//      Lz = Vz / Z
//      let A = -W/Z
//      Lz = A / (NDCz + Z)
//
//  Let's consider orthogonal projections:
//          [ X   0   0   U ]
//          [ 0   Y   0   V ]
//          [ 0   0   Z   W ]
//          [ 0   0   0   1 ]
//
//      (Note that U and V don't fit into the minimal projection!)
//
//      NDCz = Pz / Pw
//      Pz = Z * Vz + W
//      Pw = 1
//      NDCz = Pz
//           = Z * Vz + W
//
//      NDCz - W = Z * Vz
//      Vz = (NDCz - W) / Z
//
//  Argh, written these types of functions in so many different ways for so
//  many different engines. But this will be the last time...! You know why?
//  Because it's open-source!
//

struct MiniProjZW
{
    float Z;
    float W;
};

MiniProjZW AsMiniProjZW(float4 minimalProjection)
{
    MiniProjZW result;
    result.Z = minimalProjection.z;
    result.W = minimalProjection.w;
    return result;
}

MiniProjZW AsMiniProjZW(float2 minimalProjectionZW)
{
    MiniProjZW result;
    result.Z = minimalProjectionZW.x;
    result.W = minimalProjectionZW.y;
    return result;
}

MiniProjZW GlobalMiniProjZW()
{
    return AsMiniProjZW(MinimalProjection);
}

///////////////////////////////////////////////////////////////////////////////
    //      P E R S P E C T I V E       //
///////////////////////////////////////////////////////////////////////////////

    float NDCDepthToWorldSpace_Perspective(float NDCz, MiniProjZW miniProj)
    {
	    return -miniProj.W / (NDCz + miniProj.Z);
    }

    float WorldSpaceDepthToNDC_Perspective(float worldSpaceDepth, MiniProjZW miniProj)
    {
	    return -miniProj.Z - miniProj.W / worldSpaceDepth;
    }

    float NDCDepthToLinear0To1_Perspective(float NDCz, MiniProjZW miniProj)
    {
            // note -- could be optimised by pre-calculating "A" (see above)
	    return NDCDepthToWorldSpace_Perspective(NDCz, miniProj) / miniProj.Z;
    }

    float Linear0To1DepthToNDC_Perspective(float worldSpaceDepth, MiniProjZW miniProj)
    {
            // note -- could be optimised by pre-calculating "A" (see above)
	    return WorldSpaceDepthToNDC_Perspective(worldSpaceDepth * miniProj.Z, miniProj);
    }

///////////////////////////////////////////////////////////////////////////////
    //      O R T H O G O N A L     //
///////////////////////////////////////////////////////////////////////////////

    float NDCDepthToWorldSpace_Ortho(float NDCz, MiniProjZW miniProj)
    {
        return (NDCz - miniProj.W) / miniProj.Z;
    }

    float WorldSpaceDepthToNDC_Ortho(float worldSpaceDepth, MiniProjZW miniProj)
    {
	    return miniProj.Z * worldSpaceDepth + miniProj.W;
    }

    float NDCDepthDifferenceToWorldSpace_Ortho(float ndcDepthDifference, MiniProjZW miniProj)
    {
        return ndcDepthDifference / miniProj.Z;
    }

///////////////////////////////////////////////////////////////////////////////
    //      D E F A U L T S     //
///////////////////////////////////////////////////////////////////////////////

    float NDCDepthToWorldSpace(float NDCz)
    {
	    return NDCDepthToWorldSpace_Perspective(NDCz, GlobalMiniProjZW());
    }

    float WorldSpaceDepthToNDC(float worldSpaceDepth)
    {
	    return WorldSpaceDepthToNDC_Perspective(worldSpaceDepth, GlobalMiniProjZW());
    }

    float NDCDepthToLinear0To1(float NDCz)
    {
        return NDCDepthToLinear0To1_Perspective(NDCz, GlobalMiniProjZW());
    }

    float Linear0To1DepthToNDC(float worldSpaceDepth)
    {
        return Linear0To1DepthToNDC_Perspective(worldSpaceDepth, GlobalMiniProjZW());
    } 

///////////////////////////////////////////////////////////////////////////////

float3 CalculateWorldPosition(
    float3 viewFrustumVector, float linear0To1Depth,
    float3 viewPosition)
{
    return viewPosition + viewFrustumVector * linear0To1Depth;
}

#endif
