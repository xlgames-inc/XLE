// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Transform.h"

float4 CubicSCurve4(float4 x)        { return x * x * ( 3.0f - 2.0f * x ); }
float4 TriangleWave4(float4 x)       { return abs( frac( x + 0.5f ) * 2.0f - 1.0f ); }

float3 PerformWindBending(
    float3 posWorld, float3 normalWorld, float3 objCentreWorld,
    float3 instanceWindVector, float3 animParams)
{
    #if (MAT_VCOLOR_IS_ANIM_PARAM!=0) && (GEO_HAS_COLOUR!=0)
        // Distort the input position with the wind
        // Derived from method freely published in GPU Gems 3.
        //  See:
        //      http://http.developer.nvidia.com/GPUGems3/gpugems3_ch16.html
        //
        // The animParams member should be a per-vertex parameters
        // for the animation. Usually this will come from the vertex colours
        // Here R, G & B are parameters that allow customisation and
        // variation of the wind effect.
        //
        // instanceWindVector should be a vector that represents the overall
        // direction and strength of the wind. This should be equal for all
        // vertices within an object instance. Normally it would be calculated
        // by the CPU and passed via constant buffers.
        //
        // Note that only objects with vertex colours (and the material parameter
        // MAT_VCOLOR_IS_ANIM_PARAM) will get this bending animation applied.
        // Typically we want this for vegetation objects like trees and grass.
        //
        // As per the GPU Gems approach, the base animation is described by
        // triangle waves, which are fed through an "S-curve" equation to smooth
        // them out a bit.
        //
        // Note that many objects don't want the "branch" animation part at all.
        // This is counter productive for grass objects. So it may be helpful to
        // have a shader define to enable/disable this!

        float detailPhase = dot(posWorld.xyz, 20.0.xxx);
        float branchPhase = detailPhase;

        float speed = 0.125f;
        float2 timeParam = Time + float2(detailPhase, branchPhase);

            // Here, .xy are the two "edge" components
            // .zw are the two "branch" components
            // The "baseFreq" constant gives us the core ratios
            // between the animations and "baseAmpRatio" gives
            // an amplitude ratio between the slow and fast waves
            // It can help to give the slower wave a large amplitude.
        const float4 baseFreq = float4(1.975f, 0.793f, 0.375f, 0.193f);
        const float baseAmpRatio = 1.66f;
        float4 vWaves = (frac(timeParam.xxyy * baseFreq * speed) * 2.0f - 1.0f);

        vWaves = CubicSCurve4(TriangleWave4(vWaves));
        float2 vWavesSum = vWaves.xz + baseAmpRatio * vWaves.yw;

        const float baseStrength = .2f;
            // Attenuate detail movement near z=0 in object local space
            // This should clamp the vertices on the bottom of the object
            // into place, so they don't slide about the terrain.
        float detailAmp = saturate(3.f * (posWorld.z - objCentreWorld.z));
        detailAmp *= baseStrength * (1.f - animParams.g);
        float branchAmp = 0.15f * (1.f - animParams.b);

        posWorld.xyz += vWavesSum.xxy * float3(detailAmp * normalWorld.xy, branchAmp);

    #endif

    return posWorld;
}
