// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../TransformAlgorithm.h"
#include "../TextureAlgorithm.h"
#include "../CommonResources.h"
#include "../gbuffer.h"

Texture2D_MaybeMS<float4>	GBuffer_Diffuse		: register(t0);
Texture2D<float4>			DownSampledNormals	: register(t1);
Texture2D<float>			DownSampledDepth		: register(t2);
Texture2D<float>			ReflectionsMask		: register(t3);
Texture2D<float>			RandomNoiseTexture	: register(t4);

#include "ReflectionUtility.h"

RWTexture2D<float4>			OutputTexture;

static const float4 EmptyPixel = float4(.25.xxx, 0);

// #define DEBUG_STEP_COUNT

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float CalculateAlphaValue(float distance, float2 texCoord)
{
		//		We must fade off extreme samples
		//			-	fade off when the sample point is
		//				near the edge of the screen (these
		//				samples may disappear soon as the camera
		//				moves)
		//			-   fade off if we neared the edge of the
		//				ray iteration distance
	float2 t	= min(texCoord.xy, 1.f-texCoord.xy);
	t = saturate(15.f*t);
	float d = lerp(0.25f, 1.f, t.x*t.y);
	const float FadeOffDistance = 0.35f;
	d *= 1.0f - saturate((distance - (1.0f-FadeOffDistance)) * (1.0f/FadeOffDistance));
	return d;
}

float4 BuildResult(float distance, float2 texCoord, bool isGoodIntersection, uint msaaSampleIndex)
{
	const bool debugAlphaValue = false;
	#if INTERPOLATE_SAMPLES != 0
		const bool writeTextureCoordinates = true;
	#else
		const bool writeTextureCoordinates = false;
	#endif

	if (debugAlphaValue) {
		return float4(CalculateAlphaValue(distance, texCoord).xxx, 1.f);
	} else if (writeTextureCoordinates) {
		float intersectionQuality = float(isGoodIntersection) * CalculateAlphaValue(distance, texCoord);
		return float4(texCoord, intersectionQuality, 1.f);
	} else {
		float alpha = CalculateAlphaValue(distance, texCoord);
		float3 colourToReflect = 0.0.xxx;
		if (isGoodIntersection) {
			colourToReflect = SampleFloat4(GBuffer_Diffuse, ClampingSampler, texCoord, msaaSampleIndex).rgb;
		}
		return float4(colourToReflect, alpha);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//		iteration in projection space or view space						//

float4 DetailedStep(float4 start, float4 end,
					float startDistance, float endDistance,
					float finalDepthDifference,
					float2 outputDimensions, uint msaaSampleIndex)
{
	float4 stepVector = (end-start) * (1.f/float(DetailStepCount+1));
	float4 iPosition = start;
	for (uint step=0; step<DetailStepCount; ++step) {
		iPosition += stepVector;

		float4 positionProjSpace = IteratingPositionToProjSpace(iPosition);

		float depthDifference = CalculateDepthDifference(positionProjSpace, outputDimensions);
		if (depthDifference > DepthMinThreshold) {
			#if defined(DEBUG_STEP_COUNT)
				return float4((step / float(DetailStepCount)).xxx, 1);
			#endif
			float distance = lerp(startDistance, endDistance, step/float(DetailStepCount));
			float2 texCoord = AsTexCoord(positionProjSpace.xy/positionProjSpace.w);
			return BuildResult(distance, texCoord, depthDifference < DepthMaxThreshold, msaaSampleIndex);
		}
	}

	float4 endProjSpace = IteratingPositionToProjSpace(end);

		//	If we didn't hit it previously... we must hit at the "end" point
	float2 texCoord = AsTexCoord(endProjSpace.xy/endProjSpace.w);
	return BuildResult(endDistance, texCoord, finalDepthDifference < DepthMaxThreshold, msaaSampleIndex);
}

float4 FinalDetailedStep(	float4 start, float4 end,
							float startDistance, float endDistance,
							float2 outputDimensions, uint msaaSampleIndex)
{
	float4 stepVector = (end-start) * (1.f/float(DetailStepCount+1));
	float4 iPosition = start;
	[loop] for (uint step=0; step<DetailStepCount; ++step) {
		iPosition += stepVector;

		float4 positionProjSpace = IteratingPositionToProjSpace(iPosition);
		if (!WithinClipCube(positionProjSpace)) {
			break;
		}

		float depthDifference = CalculateDepthDifference(positionProjSpace, outputDimensions);
		if (depthDifference > DepthMinThreshold) {
			#if defined(DEBUG_STEP_COUNT)
				return float4((step / float(DetailStepCount)).xxx, 1);
			#endif
			float distance = lerp(startDistance, endDistance, step/float(DetailStepCount));
			float2 texCoord = AsTexCoord(positionProjSpace.xy/positionProjSpace.w);
			return BuildResult(distance, texCoord, depthDifference < DepthMaxThreshold, msaaSampleIndex);
		}
	}

		// in this case, reaching the end means there is no intersection
	return EmptyPixel;
}

float4 MultiResolutionStep(float4 startPosition, float4 basicStepSize, float randomizerValue, float2 outputDimensions, uint msaaSampleIndex)
{
	const float skipPixels	= TotalDistanceMaskShader/float(InitialStepCount);
	float4 iPosition = startPosition;

	const bool doSimpleTest = false;
	if (doSimpleTest) {
		return FinalDetailedStep(
			startPosition, startPosition + skipPixels * InitialStepCount * basicStepSize,
			0.f, 1.f,
			outputDimensions, msaaSampleIndex);
	}

	const float randomScale = lerp(0.75f, 1.f, randomizerValue);
	const uint finalStepCount = uint(InitialStepCount * randomScale);
	const float distanceValueScale = randomScale;
	float4 stepVector = basicStepSize * skipPixels * randomScale;
	float iDistance = 0.f;

	[loop] for (uint step=0; step<finalStepCount; ++step) {

 		float4 testStart = iPosition;
		float distanceStart = iDistance;
		iDistance = pow((step+1) / float(finalStepCount), IteratingPower);
		iPosition = startPosition + stepVector * (iDistance * float(finalStepCount));

		float4 positionProjSpace = IteratingPositionToProjSpace(iPosition);

			//	check to see if we've gone out of the clip cube
		if (!WithinClipCube(positionProjSpace)) {
			return FinalDetailedStep(testStart, iPosition, distanceStart * distanceValueScale, iDistance * distanceValueScale, outputDimensions, msaaSampleIndex);
		} else {
			float depthDifference = CalculateDepthDifference(positionProjSpace, outputDimensions);
			if (depthDifference > DepthMinThreshold) {
				return DetailedStep(
					testStart, iPosition,
					distanceStart * distanceValueScale, iDistance * distanceValueScale,
					depthDifference,
					outputDimensions, msaaSampleIndex);
			}
		}
 	}

	return EmptyPixel;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//		iteration on per-pixel basis						//

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
        float queryDepth = DownSampledDepth[pixelCapCoord];
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
	const float continuousEpsilon = 0.001f;
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

float4 PixelBasedIteration(ReflectionRay ray, float randomizerValue, float2 outputDimensions)
{
		//
		//		We're going to iterate across this ray on a per pixel basis.
		//		We want to use a modified Bressenham algorithm for this. The
		//		normal Bressenham won't find every pixel that this ray enters.
		//		We want to find every pixel, even if the ray doesn't go near
		//		the centre of that pixel...
		//
		//		We don't want to interpolate between depth values in the depth
		//		texture, because interpolation between depth values can cause
		//		strange results (particularly if the depth values are NDC coordinates
		//		-- we can't do linear interpolation on those coordinate).
		//
		//		But we can interpolate between depth values along the ray. So
		//		the ideal ray test is this:
		//			* find the points that the ray enters and exits each pixel
		//			* those 2 points should define a small depth range.
		//			* we want to see if the ray straddles the depth of that pixel
		//

		//		(the actual dimensions of w & h don't really matter to much.
		//			high numbers mean more accurate interpolation when the ray
		//			doesn't pass exactly through pixel centers)

	const uint pixelStep = 4;
	const uint initialPixelsSkip = 2 + int(randomizerValue * (pixelStep+1));

	int w = int(float(ReflectionDistancePixels) * ray.screenSpaceRayDirection.x);
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
	iterator._gotIntersection = false;
	iterator._continueIteration = true;
	iterator._intersectionCoords = float2(-1.f, -1.f);
	iterator._testCount = 0;

	float2 postDivide =
		float2(	((x * 2.0f - 0.5f) / outputDimensions.x) - 1.f,
				((y * -2.0f + 0.5f) / outputDimensions.y) + 1.f);
	iterator._pixelEntryZW = (ddx >= ddy)?DepthCalcX(postDivide, iterator._ray):DepthCalcY(postDivide, iterator._ray);

	#if defined(DEPTH_IN_LINEAR_COORDS)
		// not implemented
	#else
		iterator._lastQueryDepth = DownSampledDepth[int2(x, y)];
	#endif

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

	if (!iterator._gotIntersection) return EmptyPixel;
	// return float4((iterator._testCount / 256.f).xxx, 1.0.x);		// (display step count)

	return BuildResult(
		distance,
		iterator._intersectionCoords.xy / float2(outputDimensions),
		iterator._gotIntersection, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

[numthreads(16, 16, 1)]
	void BuildReflection(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	float maskValue = ReflectionsMask[dispatchThreadId.xy];
	[branch] if (maskValue < 0.01f) {
		OutputTexture[dispatchThreadId.xy] = 0.0.xxxx;
		return;
	}

		// do high resolution ray sample
	const uint msaaSampleIndex = 0;
	uint2 outputDimensions;
	DownSampledDepth.GetDimensions(outputDimensions.x, outputDimensions.y);

	float4 result;
	ReflectionRay ray = CalculateReflectionRay(dispatchThreadId.xy, outputDimensions, msaaSampleIndex);
	if (ray.valid) {
			//	when not doing the blur, the jittering actually looks better with
			//	a repeating pattern. For some reason, the regularity of it helps
			//	a completely random pattern just ends up looking messy and distracting
		// float randomizerValue = RandomNoiseTexture[dispatchThreadId.xy & 0x3]; // 0xff];

		int ditherArray[16] =
		{
			 4, 12,  0,  8,
			10,  2, 14,  6,
			15,  7, 11,  3,
			 1,  9,  5, 13
		};
		uint2 t = dispatchThreadId.xy & 0x3;
		float randomizerValue = float(ditherArray[t.x+t.y*4]) / 15.f;

		const uint stepMethod = 1;
		if (stepMethod == 0) {
			result = MultiResolutionStep(
				ray.projStartPosition, ray.projBasicStep,
				randomizerValue, float2(outputDimensions), msaaSampleIndex);
		} else if (stepMethod == 1) {
			result = PixelBasedIteration(ray, randomizerValue, float2(outputDimensions));
		}
		// result.rgb = 16.f * ray.basicStep.rgb;
		// result.a *= maskValue;		// we need this to pick up the fresnel calculation made in BuildMask.csh
	} else {
		result = EmptyPixel;
	}

	OutputTexture[dispatchThreadId.xy] = result;
	// OutputTexture[dispatchThreadId.xy] = float4(maskValue.xxx, 1.f);
}
