// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(PROJECTION_MATH_H)
#define PROJECTION_MATH_H

bool InsideFrustum(float4 clipSpacePosition)
{
	float3 p = clipSpacePosition.xyz / clipSpacePosition.w;
		// arrange all coordinates so that values inside of the frustum are < 1.f
	float2 t = abs(p.xy);
	float t2 = max(1.f-p.z, p.z);
	float t3 = max(t2, max(t.x, t.y));
	return t3 < 1.f;	// if the maximum is less than 1.f, then all must be less than 1.f
}

#endif

