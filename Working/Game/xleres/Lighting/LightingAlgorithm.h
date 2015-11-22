// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(LIGHTING_ALGORITHM_H)
#define LIGHTING_ALGORITHM_H

#include "../CommonResources.h"
#include "../Utility/MathConstants.h"

float Square(float x) { return x*x; }

float RefractiveIndexToF0(float refractiveIndex)
{
        // (note -- the 1.f here assumes one side of the interface is air)
	return Square((refractiveIndex - 1.f) / (refractiveIndex + 1.f));
}

float SchlickFresnelF0(float3 viewDirection, float3 halfVector, float F0)
{
		// Note that we're using the half vector as a parameter to the fresnel
		// equation. See also "Physically Based Lighting in Call of Duty: Black Ops"
		// (Lazarov/Treyarch) for another example of this. The theory is this:
		//      If we imagine the surface as being microfacetted, then the facets
		//      that are reflecting light aren't actually flat on the surface... They
		//      are the facets that are raised towards the viewer. Our "halfVector" is
		//      actually an approximation of an average normal of these active facets.
		// For a perfect mirror surface, maybe we could consider using the normal
		// instead of the half vector here? Might be nice for intense reflections
		// from puddles of water, etc. Maybe we could also just use the roughness
		// value to interpolate between half vector and normal...?
		//
		// We can also consider a "spherical gaussian approximation" for fresnel. See:
		//	https://seblagarde.wordpress.com/2012/06/03/spherical-gaussien-approximation-for-blinn-phong-phong-and-fresnel/
		//	pow(1 - dotEH, 5) = exp2((-5.55473 * EdotH - 6.98316) * EdotH)
		// It seems like the performance difference might be hardware dependent
		// Also, maybe we should consider matching the gaussian approximation to the full
		// fresnel equation; rather than just Schlick's approximation.
	float A = 1.0f - saturate(dot(viewDirection, halfVector));
	float sq = A*A;
	float cb = sq*sq;
	float q = cb*A;	// we could also consider just using the cubed value here
	return F0 + (1.f - F0) * q;	// (note, use lerp for this..?)
}

float SchlickFresnelF0_Modified(float3 viewDirection, float3 halfVector, float F0)
{
		// In this modified version, we attempt to reduce the extreme edges of
		// the reflection by imposing an upper limit.
		// The extreme edges of the reflection can often highlight rendering
		// inaccuracies (such as lack of occlusion and local reflections).
		// So, oftening it off helps to reduce problems.
	float A = 1.0f - saturate(dot(viewDirection, halfVector));
	float sq = A*A;
	float cb = sq*sq;
	float q = cb*A;

	float upperLimit = min(1.f, 50.f * (F0+0.001f));
	return F0 + (upperLimit - F0) * q;
}

float3 SchlickFresnelF0(float3 viewDirection, float3 halfVector, float3 F0)
{
	float A = 1.0f - saturate(dot(viewDirection, halfVector));
	float sq = A*A;
	float cb = sq*sq;
	float q = cb*A;
	return lerp(F0, 1.f, q);
}

float3 SchlickFresnelF0_Modified(float3 viewDirection, float3 halfVector, float3 F0)
{
	float A = 1.0f - saturate(dot(viewDirection, halfVector));
	float sq = A*A;
	float cb = sq*sq;
	float q = cb*A;

	float3 upperLimit = min(1.f, 50.f * (F0+0.001f));
	return F0 + (upperLimit - F0) * q;
}

float SchlickFresnel(float3 viewDirection, float3 halfVector, float refractiveIndex)
{
    float F0 = RefractiveIndexToF0(refractiveIndex);
    return SchlickFresnelF0(viewDirection, halfVector, F0);
}

float CalculateMipmapLevel(float2 texCoord, uint2 textureSize)
{
		// Based on OpenGL 4.2 spec chapter 3.9.11 equation 3.21
		// This won't automatically match the results given by all
		// hardware -- but it should be a good alternative to the
		// built in hardware mipmap calculation when an explicit
		// formula is required.
	float2 et = abs(texCoord * textureSize);
	float2 dx = ddx(et), dy = ddy(et);
	float d = max(dot(dx, dx), dot(dy, dy));
	return 0.5f*log2(d); // == log2(sqrt(d))
}

void OrenNayar_CalculateInputs(float roughness, out float rho, out float shininess)
{
	    // rho = roughness * roughness;
	    // shininess = 1.f - roughness;
    rho = 1.f;
    shininess = 2.0f / (roughness*roughness);
}

float4 ReadReflectionHemiBox( float3 direction,
                                Texture2D face12, Texture2D face34, Texture2D face5,
                                uint2 textureSize, uint defaultMipMap)
{
	float3 absDirection = abs(direction);
	direction.z = absDirection.z;

	float2 tc;
    uint textureIndex = 0;

		// Simple non-interpolated lookup to start
	[branch] if (absDirection.z > absDirection.x && absDirection.z > absDirection.y) {
		tc = 0.5f + 0.5f * direction.xy / direction.z;
        textureIndex = 2;
	} else if (absDirection.x > absDirection.y && absDirection.x > absDirection.z) {
		tc = 0.5f + 0.5f * direction.yz / absDirection.x;
		if (direction.x > 0.f) {
			tc.x = 1.f-tc.x;
			textureIndex = 0;
		} else {
			textureIndex = 1;
		}
	} else {
		tc = 0.5f + 0.5f * direction.xz / absDirection.y;
		tc.y = 1.f-tc.y;
		if (direction.y > 0.f) {
			textureIndex = 0;
		} else {
			tc.x = 1.f-tc.x;
			textureIndex = 1;
		}
	}

        // note --  mipmap calculation is incorrect on edges, where
        //          we flip from one texture to another.
        //          It's a problem... let's just use point sampling for now
    const bool usePointSampling = true;
    if (usePointSampling) {
        uint mipMap = defaultMipMap;
        int2 t = saturate(tc)*textureSize;
        t >>= mipMap;
        [branch] if (textureIndex == 2) {
            return face5.Load(int3(t, mipMap));
	    } else if (textureIndex == 0) {
		    return face12.Load(int3(t, mipMap));
	    } else {
		    return face34.Load(int3(t, mipMap));
	    }
    } else {
        uint mipMap = CalculateMipmapLevel(tc, textureSize);
        [branch] if (textureIndex == 2) {
            return face5.SampleLevel(ClampingSampler, tc, mipMap);
	    } else if (textureIndex == 0) {
		    return face12.SampleLevel(ClampingSampler, tc, mipMap);
	    } else {
		    return face34.SampleLevel(ClampingSampler, tc, mipMap);
	    }
    }
}

float2 EquirectangularMappingCoord(float3 direction)
{
		// note -- 	the trigonometry here is a little inaccurate. It causes shaking
		//			when the camera moves. We might need to replace it with more
		//			accurate math.
	float theta = atan2(direction.x, direction.y);
	float inc = atan(direction.z * rsqrt(dot(direction.xy, direction.xy)));

	float x = 0.5f + 0.5f*(theta / (1.f*pi));
	float y = .5f-(inc / pi);
	return float2(x, y);
}

float2 HemiEquirectangularMappingCoord(float3 direction)
{
	float theta = atan2(direction.x, direction.y);
	float inc = atan(direction.z * rsqrt(dot(direction.xy, direction.xy)));

	float x = 0.5f + 0.5f*(theta / (1.f*pi));
	float y = 1.f-(inc / (.5f*pi));
	return float2(x, y);
}

float SimplifiedOrenNayer(float3 normal, float3 viewDirection, float3 lightDirection, float rho, float shininess)
{
		//
		//		Using a simplified Oren-Nayar implementation, without any lookup tables
		//		See http://blog.selfshadow.com/publications/s2012-shading-course/gotanda/s2012_pbs_beyond_blinn_notes_v3.pdf
		//		for details.
		//
		//		This is the non-spherical harmonic version (equation 25)
		//
		//		See also original Oren-Nayar paper:
		//			http://www1.cs.columbia.edu/CAVE/publications/pdfs/Oren_SIGGRAPH94.pdf
		//

	const float NL	 = dot(normal, lightDirection);
	const float NE	 = dot(normal, viewDirection);
	const float EL	 = dot(viewDirection, lightDirection);
	float A			 = EL - NE * NL;
	const float E0	 = 1.f;		// (from the Oren-Nayar paper, E0 is the irradiance when the facet is illuminated head-on)
	float B;
	/*if (A >= 0.f) {		// (better to branch or unwrap?)
		B = min(1.f, NL / NE);		(this branch is causing a wierd discontinuity. There must be something wrong with the equation)
	} else*/ {
		B = NL;
	}

	const float C = (1.0f / (2.22222f + .1f * shininess)) * A * B;
	const float D = NL * (1.0f - 1.0f / (2.f + .65 * shininess)) + C;
	return saturate(E0 * rho / pi * D);
}



#endif
