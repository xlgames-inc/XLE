// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(PROJECTION_MATH_H)
#define PROJECTION_MATH_H

bool PtInFrustum(float4 pt)
{
	float xyMax = max(abs(pt.x), abs(pt.y));
	return max(xyMax, max(pt.z, pt.w-pt.z)) <= pt.w;
}

bool InsideFrustum(float4 clipSpacePosition) { return PtInFrustum(clipSpacePosition); }

int CountTrue(bool3 input)
{
	return dot(true.xxx, input);
}

bool TriInFrustum(float4 pt0, float4 pt1, float4 pt2)
{
	float3 xs = float3(pt0.x, pt1.x, pt2.x);
	float3 ys = float3(pt0.y, pt1.y, pt2.y);
	float3 zs = float3(pt0.z, pt1.z, pt2.z);
	float3 ws = abs(float3(pt0.w, pt1.w, pt2.w));

	int l  = CountTrue(xs < -ws);
	int r  = CountTrue(xs >  ws);
	int t  = CountTrue(ys < -ws);
	int b  = CountTrue(ys >  ws);
	int f  = CountTrue(zs < 0.f);
	int bk = CountTrue(zs >  ws);

	return max(max(max(max(max(l, r), t), b), f), bk) < 3;
}

float BackfaceSign(float4 A, float4 B, float4 C)
{
	float2 a = A.xy / A.w;
	float2 b = B.xy / B.w;
	float2 c = C.xy / C.w;
	float2 edge0 = float2(b.x - a.x, b.y - a.y);
	float2 edge1 = float2(c.x - b.x, c.y - b.y);
	return (edge0.x*edge1.y) - (edge0.y*edge1.x);
}

#endif
