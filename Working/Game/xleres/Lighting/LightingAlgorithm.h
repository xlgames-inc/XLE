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
	float A = 1.0f - saturate(dot(viewDirection, halfVector));
	float sq = A*A;
	float cb = sq*sq;
	float q = cb*A;

	return F0 + (1.f - F0) * q;	// (note, use lerp for this..?)
}

float SchlickFresnel(float3 viewDirection, float3 halfVector, float refractiveIndex)
{
    float F0 = RefractiveIndexToF0(refractiveIndex);
    return SchlickFresnelF0(viewDirection, halfVector, F0);
}

float CalculateMipmapLevel(float2 texCoord, uint2 textureSize)
{
	float2 dx = ddx(texCoord * float(textureSize.x));
	float2 dy = ddy(texCoord * float(textureSize.y));
	float d = max(dot(dx, dx), dot(dy, dy));
	return 0.5f*log2(d);
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

float2 HemisphericalMappingCoord(float3 direction)
{
	float2 a = normalize(direction.xy);
	float theta = atan2(a.x, a.y);
	float inc = atan(direction.z/length(direction.xy));

	float x = 0.5f + 0.5f*(theta / (1.f*pi));

	float y = 1.f-(inc / (.5f*pi));
	y /= 2.f;
    // y /= 1.1f;

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

