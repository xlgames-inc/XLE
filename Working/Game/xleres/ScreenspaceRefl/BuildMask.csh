// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../TransformAlgorithm.h"
#include "../TextureAlgorithm.h"
#include "../gbuffer.h"
#include "../Lighting/LightingAlgorithm.h"

RWTexture2D<float>			ReflectionsMask;
Texture2D<float4>			DownSampledNormals	: register(t1);
Texture2D<float>			DepthTexture		: register(t2);
Texture2D<float>			RandomNoiseTexture	: register(t4);

#include "ReflectionUtility.h"

static const uint SamplesPerBlock = 64;
static const uint BlockDimension = 64;

cbuffer SamplingPattern
{
	uint4 SamplePoint[SamplesPerBlock];
	uint4 ClosestSamples[BlockDimension][BlockDimension/4];
	uint4 ClosestSamples2[BlockDimension][BlockDimension/4];
};

///////////////////////////////////////////////////////////////////////////////////////////

struct PBI
{
	ReflectionRay	_ray;
	float2			_pixelEntryZW;
	float			_lastQueryDepth;

	bool		_gotIntersection;
	bool		_continueIteration;
	float2		_intersectionCoords;
	int			_testCount;
};

float2 DepthCalcX(float2 postDivide, ReflectionRay ray)
{
		//	here, we're calculating the z and w values of the given ray
		//	for a given point in screen space. This should be used for
		//	"x" dominant rays.
	float a =		(postDivide.x * ray.projStartPosition.w - ray.projStartPosition.x)
				/	(ray.projBasicStep.x - postDivide.x * ray.projBasicStep.w);
	return ray.projStartPosition.zw + a * ray.projBasicStep.zw;
}

float2 DepthCalcY(float2 postDivide, ReflectionRay ray)
{
		//	here, we're calculating the z and w values of the given ray
		//	for a given point in screen space. This should be used for
		//	"y" dominant rays.
	float a =		(postDivide.y * ray.projStartPosition.w - ray.projStartPosition.y)
				/	(ray.projBasicStep.y - postDivide.y * ray.projBasicStep.w);
	return ray.projStartPosition.zw + a * ray.projBasicStep.zw;
}

void PBI_Opr(inout PBI iterator, float2 exitZW, int2 pixelCapCoord, float2 edgeCoords)
{
	float2 entryZW = iterator._pixelEntryZW;
	iterator._pixelEntryZW = exitZW;
	++iterator._testCount;

		//	We now know the depth values where the ray enters and exits
		//	this pixel.
		//	We can compare this to the values in the depth buffer
		//	to look for an intersection
	float ndc0 = entryZW.x / entryZW.y;
	float ndc1 =  exitZW.x /  exitZW.y;

		//	we have to check to see if we've left the view frustum.
		//	going too deep is probably not likely, but we can pass
		//	in front of the near plane
	if (ndc1 <= 0.f) {
		iterator._continueIteration = false;
		iterator._intersectionCoords = 0.0.xx;
		return;
	}

	#if defined(DEPTH_IN_LINEAR_COORDS)
		// not implemented
    #else
        float queryDepth = DepthTexture[pixelCapCoord];
	#endif
	float lastQueryDepth = iterator._lastQueryDepth;
	iterator._lastQueryDepth = queryDepth;

		//	collide first with the "cap" of the pixel. If the ray
		//	penetrates the pixel cap, we know that there is definitely
		//	an intersection
	const float epsilon = 0.f;
//	if (	queryDepth >= min(ndc0, ndc1) - epsilon
//		&&	queryDepth <= max(ndc0, ndc1) + epsilon) {

		//	try to distinquish front-face and back-face collisions by looking at
		//	the direction we pass through this pixel cap
	if (ndc0 <= queryDepth && ndc1 >= queryDepth) {
		iterator._gotIntersection = true;
		iterator._continueIteration = false;
		iterator._intersectionCoords = float2(pixelCapCoord);
	}

		//	As collide with the edge of the pixel. Sometimes an edge in depth
		//	space represents a continuous edge in world space. But other times,
		//	there is a continiuity there. We can use an epsilon value to try to
		//	distinquish the two.
		//	Note that if the ray intersects a discontinuous edge, we have 2 options:
		//		* finish the ray here, and create a "shadow" in the reflection
		//		* just continue the ray behind the object, and look for a high
		//			quality intersection later.
		//
		//	Also note that there are two types of intersections: back-face and front-face.
		//	We don't really want to stop at a back-face intersection
	const float continuousEpsilon = 0.005f;	// (have to be rough for build mask)
	bool isFrontFace = true; // (ndc1 > ndc0) != (queryDepth > lastQueryDepth);
	if (abs(lastQueryDepth - queryDepth) < continuousEpsilon && isFrontFace) {
		if (ndc0 >= min(lastQueryDepth, queryDepth) && ndc0 <= max(lastQueryDepth, queryDepth)) {
			iterator._gotIntersection = true;
			iterator._continueIteration = false;
			iterator._intersectionCoords = edgeCoords;
		}
	}
}

void PBI_OprX(inout PBI iterator, int2 e0, int2 e1, float alpha, int2 pixelCoord, float2 outputDimensions)
{
	float2 edgeIntersection = lerp(float2(e0), float2(e1), alpha);
	float2 postDivide =
		float2(	(edgeIntersection.x *  2.0f - 0.5f) / outputDimensions.x - 1.f,
				(edgeIntersection.y * -2.0f + 0.5f) / outputDimensions.y + 1.f);
	float2 exitZW = DepthCalcX(postDivide, iterator._ray);
	PBI_Opr(iterator, exitZW, pixelCoord, edgeIntersection);
}

void PBI_OprY(inout PBI iterator, int2 e0, int2 e1, float alpha, int2 pixelCoord, float2 outputDimensions)
{
	float2 edgeIntersection = lerp(float2(e0), float2(e1), alpha);
	float2 postDivide =
		float2(	(edgeIntersection.x *  2.0f - 0.5f) / outputDimensions.x - 1.f,
				(edgeIntersection.y * -2.0f + 0.5f) / outputDimensions.y + 1.f);
	float2 exitZW = DepthCalcY(postDivide, iterator._ray);
	PBI_Opr(iterator, exitZW, pixelCoord, edgeIntersection);
}

bool PixelBasedIteration(ReflectionRay ray, float randomizerValue, float2 outputDimensions)
{
	const uint pixelStep = 8;
	const uint initialPixelsSkip = 2 + int(randomizerValue * (pixelStep+1));

	int w = int( float(ReflectionDistancePixels) * ray.screenSpaceRayDirection.x);
	int h = int(-float(ReflectionDistancePixels) * ray.screenSpaceRayDirection.y);

	int ystep = sign(h); h = abs(h);
	int xstep = sign(w); w = abs(w);
	int ddy = 2 * h;  // We may not need to double this (because we're starting from the corner of the pixel)
	int ddx = 2 * w;

	int i=0;
	int errorprev = 0, error = 0; // (start from corner. we don't want to start in the middle of the grid element)
	int x = int(((ray.projStartPosition.x / ray.projStartPosition.w) * .5f + .5f) * outputDimensions.x),
		y = int(((ray.projStartPosition.y / ray.projStartPosition.w) * -.5f + .5f) * outputDimensions.y);

			//	step 2 pixel forward
			//		this helps avoid bad intersections greatly. The first pixel is the starter pixel,
			//		the second pixel is the first pixel "cap" we'll test
			//	So we must skip at least 2 pixels before iteration. After that, we can offset
			//	based on a random value -- this will add some noise, but will cover the big
			//	gap between pixel steps.
	if (ddx >= ddy) {
		x += initialPixelsSkip * xstep;
		y += ystep * ddy / ddx;
		errorprev = error = ddy % ddx;
		i += initialPixelsSkip;
	} else {
		y += initialPixelsSkip * ystep;
		x += xstep * ddx / ddy;
		errorprev = error = ddx % ddy;
		i += initialPixelsSkip;
	}

		//	We don't have to visit every single pixel.
		//	use "pixel step" to jump over some pixels. It adds some noise, but not too bad.
	xstep *= pixelStep;
	ystep *= pixelStep;

	PBI iterator;
	iterator._ray = ray;
	iterator._gotIntersection		= false;
	iterator._continueIteration		= true;
	iterator._intersectionCoords	= float2(-1.f, -1.f);
	iterator._testCount				= 0;

	float2 postDivide =
		float2(	((x * 2.0f - 0.5f) / outputDimensions.x) - 1.f,
				((y * -2.0f + 0.5f) / outputDimensions.y) + 1.f);
	iterator._pixelEntryZW = (ddx >= ddy)?DepthCalcX(postDivide, iterator._ray):DepthCalcY(postDivide, iterator._ray);

	#if defined(DEPTH_IN_LINEAR_COORDS)
		// not implemented
	#else
		iterator._lastQueryDepth = DepthTexture[int2(x, y)];
	#endif

	// We're just going crazy with conditions and looping here!
	// Surely there must be a better way to do this!

	float distance;
	if (ddx >= ddy) {
		for (; i<w && iterator._continueIteration; ++i) {
			int2 pixelCapCoord = int2(x, y);

			x += xstep;
			error += ddy;

			int2 e0, e1;
			float edgeAlpha;

			if (error >= ddx) {

				y += ystep;
				error -= ddx;

					//  The cases for what happens here. Each case defines different edges
					//  we need to check
				if (error != 0) {
					e0 = int2(x, y); e1 = int2(x, y+ystep);
					edgeAlpha = error / float(ddx);

					int2 e0b = int2(x-xstep, y);
					int2 e1b = int2(x, y);
					int tri0 = ddx - errorprev;
					int tri1 = error;
					PBI_OprX(iterator, e0b, e1b, tri0 / float(tri0+tri1), pixelCapCoord, outputDimensions);
					if (!iterator._continueIteration) break;
				} else {
						// passes directly though the corner. Easiest case.
					e0 = e1 = int2(x, y);
					edgeAlpha = 0.f;
				}

			} else {
					// simple -- y isn't changing, just moving to the next "x" grid
				e0 = int2(x, y); e1 = int2(x, y+ystep);
				edgeAlpha = error / float(ddx);
			}

			PBI_OprX(iterator, e0, e1, edgeAlpha, int2(x, y), outputDimensions);
			errorprev = error;
		}
		distance = i / float(w);
	} else {
		for (; i<h && iterator._continueIteration; ++i) {
			int2 pixelCapCoord = int2(x, y);

			y += ystep;
			error += ddx;

			int2 e0, e1;
			float edgeAlpha;

			if (error >= ddy) {

				x += xstep;
				error -= ddy;

					//  The cases for what happens here. Each case defines different edges
					//  we need to check
				if (error != 0) {
					e0 = int2(x, y); e1 = int2(x+xstep, y);
					edgeAlpha = error / float(ddy);

					int2 e0b = int2(x, y-ystep);
					int2 e1b = int2(x, y);
					int tri0 = ddy - errorprev;
					int tri1 = error;
					PBI_OprY(iterator, e0b, e1b, tri0 / float(tri0+tri1), pixelCapCoord, outputDimensions);
					if (!iterator._continueIteration) break;
				} else {
						// passes directly though the corner. Easiest case.
					e0 = e1 = int2(x, y);
					edgeAlpha = 0.f;
				}

			} else {
					// simple -- y isn't changing, just moving to the next "x" grid
				e0 = int2(x, y); e1 = int2(x+xstep, y);
				edgeAlpha = error / float(ddy);
			}

			PBI_OprY(iterator, e0, e1, edgeAlpha, int2(x, y), outputDimensions);
			errorprev = error;
		}
		distance = i / float(h);
	}

	return iterator._gotIntersection;
}

///////////////////////////////////////////////////////////////////////////////////////////

groupshared bool	SampleFoundCollision[SamplesPerBlock];

float ReciprocalLength(float3 v) { return rsqrt(dot(v, v)); }

[numthreads(1, 1, 64)]
	void BuildMask(uint3 dispatchThreadId : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
		//
		//		Split the buffer into 64x64 pixel blocks.
		//		In each block, sample the offsets in "samplePoint"
		//

	uint2 outputDimensions;
	DepthTexture.GetDimensions(outputDimensions.x, outputDimensions.y);

	const uint msaaSampleIndex = 0;
	uint rayCastSample = groupIndex;

		//
		//		We do 1 sample ray test per thread. This avoids putting a complex
		//		loop within the shader, and should give us much better parallelism (particularly
		//		at low resolutions)
		//
	uint2 sampleOffset = SamplePoint[rayCastSample].xy;
	// sampleOffset = uint2(BlockDimension.xx * float2(frac(rayCastSample/8.f), frac((rayCastSample/8)/8.f))) + uint2(4,4);
 	uint2 samplingPixel = dispatchThreadId.xy * BlockDimension.xx + sampleOffset;

	ReflectionRay ray = CalculateReflectionRay(samplingPixel.xy, outputDimensions, msaaSampleIndex);

	bool foundCollision = false;
	[branch] if (ray.valid) {

		#if 0

			float randomizerValue = RandomNoiseTexture[dispatchThreadId.xy & 0xff];

			const float randomScale = lerp(0.5f, 1.5f, randomizerValue);
			const uint finalStepCount = uint(MaskStepCount * randomScale)+1;
			float4 stepVector = ray.projBasicStep * MaskSkipPixels / randomScale;

			const bool preClipRay = false;	// not working correctly
			if (preClipRay) {
 				float4 rayEnd = ray.projStartPosition + stepVector * finalStepCount;
				ClipRay(ray.projStartPosition, rayEnd);
				stepVector = (rayEnd - ray.projStartPosition) / float(finalStepCount);
			}

			float4 iteratingPosition = ray.projStartPosition;
			[loop] for (uint step=0; step<finalStepCount; ++step) {
 				// iteratingPosition	   += stepVector;
				iteratingPosition
					=	ray.projStartPosition
						+ stepVector * (pow((step+1) / float(finalStepCount), IteratingPower) * float(finalStepCount))
						;

				float4 positionProjSpace = IteratingPositionToProjSpace(iteratingPosition);

				if (!preClipRay && !WithinClipCube(positionProjSpace)) {
					break;
				} else if (CompareDepth(positionProjSpace, outputDimensions)) {
					foundCollision = true;
					break;
 				}
 			}

		#else

			int ditherArray[16] =
			{
				 4, 12,  0,  8,
				10,  2, 14,  6,
				15,  7, 11,  3,
				 1,  9,  5, 13
			};
			uint2 t = dispatchThreadId.xy & 0x3;
			float randomizerValue = float(ditherArray[t.x+t.y*4]) / 15.f;

			foundCollision = PixelBasedIteration(ray, randomizerValue, float2(outputDimensions));

		#endif
	}

	SampleFoundCollision[rayCastSample] = foundCollision;

		////////// BARRIER! //////////
			//		Wait until every thread in the group has completed the above sample ray test

	GroupMemoryBarrierWithGroupSync();

		//
		//		Assuming BlockDimension == SamplesPerBlock
		//		We can split each "y" iteration through this
		//		loop into a different compute shader thread
		//			(just better for parrallelism than a loop)
		//
		//		We're mostly reading from "SampleFoundCollision" here
		//			... so that must be "groupshared"
		//
	uint y = groupIndex;
	for (uint x=0; x<BlockDimension; ++x) {

			//	We know the 4 closest samples to this pixel.
			//	Find them out and find how many successful samples
		int samplesWithCollisions = 0;
		uint packedSampleIndices = ClosestSamples[y][x/4][x%4];
		uint packedSampleIndices2 = ClosestSamples2[y][x/4][x%4];
		samplesWithCollisions += SampleFoundCollision[ packedSampleIndices        & 0xff];
		samplesWithCollisions += SampleFoundCollision[(packedSampleIndices >>  8) & 0xff];
		samplesWithCollisions += SampleFoundCollision[(packedSampleIndices >> 16) & 0xff];
		samplesWithCollisions += SampleFoundCollision[(packedSampleIndices >> 24) & 0xff];

		samplesWithCollisions += SampleFoundCollision[ packedSampleIndices2        & 0xff];
		samplesWithCollisions += SampleFoundCollision[(packedSampleIndices2 >>  8) & 0xff];
		samplesWithCollisions += SampleFoundCollision[(packedSampleIndices2 >> 16) & 0xff];
		samplesWithCollisions += SampleFoundCollision[(packedSampleIndices2 >> 24) & 0xff];

		const uint2 samplingPixel = dispatchThreadId.xy * BlockDimension.xx + uint2(x, y);

		float maskValue;
			[branch] if (samplesWithCollisions > 0) {

				//	At least one successful collisions means we should consider
				//	this an active pixel.
			const float SampleDivisor = 4.f;
			const float maskFromCollision = saturate(float(samplesWithCollisions)/SampleDivisor);

			float linearDepth;
			float3 worldSpacePosition = CalculateWorldSpacePosition(
				samplingPixel, outputDimensions, msaaSampleIndex, linearDepth);

			float3 worldSpaceNormal = DownSampledNormals[samplingPixel].rgb;
			float F0 = DownSampledNormals[samplingPixel].a;
			if (dot(worldSpaceNormal, worldSpaceNormal) < 0.25f || F0 < 0.05f) {
				maskValue = 0.f;	// invalid edge pixels have normals with tiny magnitudes
			} else {
				// We're just going to do a quick estimate of the fresnel term by
				// using the angle between the view direction and the normal. This is
				// equivalent to mirror reflections model -- it's not perfectly
				// right for our microfacet reflections model.
				// But should be ok for an estimate here.

				float3 viewDirection = WorldSpaceView - worldSpacePosition;
				float NdotV = dot(viewDirection, worldSpaceNormal) * ReciprocalLength(viewDirection); // (correct so long as worldSpaceNormal is unit length)
				float q = SchlickFresnelCore(saturate(NdotV));
				float F = F0 + (1.f - F0) * q;
				maskValue = F * maskFromCollision;
			}
		} else {
			maskValue = 0.f;
		}

		ReflectionsMask[samplingPixel] = maskValue;
	}

}
