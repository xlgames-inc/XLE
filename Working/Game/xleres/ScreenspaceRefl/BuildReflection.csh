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
#include "PixelBasedIteration.h"

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

		float4 clipSpace = IteratingPositionToClipSpace(iPosition);
		float depthTextureValue = LoadDepth(clipSpace.xy/clipSpace.w, outputDimensions);
		float depthDifference = DepthDifference(clipSpace.z/clipSpace.w, depthTextureValue);
		if (depthDifference > DepthMinThreshold) {
			#if defined(DEBUG_STEP_COUNT)
				return float4((step / float(DetailStepCount)).xxx, 1);
			#endif
			float distance = lerp(startDistance, endDistance, step/float(DetailStepCount));
			float2 texCoord = AsZeroToOne(clipSpace.xy/clipSpace.w);
			return BuildResult(distance, texCoord, depthDifference < DepthMaxThreshold, msaaSampleIndex);
		}
	}

	float4 endClipSpace = IteratingPositionToClipSpace(end);

		//	If we didn't hit it previously... we must hit at the "end" point
	float2 texCoord = AsZeroToOne(endClipSpace.xy/endClipSpace.w);
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

		float4 clipSpace = IteratingPositionToClipSpace(iPosition);
		if (!WithinClipCube(clipSpace)) {
			break;
		}

		float depthTextureValue = LoadDepth(clipSpace.xy/clipSpace.w, outputDimensions);
		float depthDifference = DepthDifference(clipSpace.z/clipSpace.w, depthTextureValue);
		if (depthDifference > DepthMinThreshold) {
			#if defined(DEBUG_STEP_COUNT)
				return float4((step / float(DetailStepCount)).xxx, 1);
			#endif
			float distance = lerp(startDistance, endDistance, step/float(DetailStepCount));
			float2 texCoord = AsZeroToOne(clipSpace.xy/clipSpace.w);
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

		float4 clipSpace = IteratingPositionToClipSpace(iPosition);

			//	check to see if we've gone out of the clip cube
		if (!WithinClipCube(clipSpace)) {
			return FinalDetailedStep(testStart, iPosition, distanceStart * distanceValueScale, iDistance * distanceValueScale, outputDimensions, msaaSampleIndex);
		} else {
			float depthTextureValue = LoadDepth(clipSpace.xy/clipSpace.w, outputDimensions);
			float depthDifference = DepthDifference(clipSpace.z/clipSpace.w, depthTextureValue);
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

uint IterationOperator(int2 pixelCapCoord, float entryNDCDepth, float exitNDCDepth, inout float ilastQueryDepth)
{
	#if defined(DEPTH_IN_LINEAR_COORDS)
		// not implemented
    #else
        float queryDepth = DownSampledDepth[pixelCapCoord];
	#endif
	float lastQueryDepth = ilastQueryDepth;
	ilastQueryDepth = queryDepth;

		//	collide first with the "cap" of the pixel. If the ray
		//	penetrates the pixel cap, we know that there is definitely
		//	an intersection
	const float epsilon = 0.f;
//	if (	queryDepth >= min(entryNDCDepth, exitNDCDepth) - epsilon
//		&&	queryDepth <= max(entryNDCDepth, exitNDCDepth) + epsilon) {

		//	try to distinquish front-face and back-face collisions by looking at
		//	the direction we pass through this pixel cap
	if (entryNDCDepth <= queryDepth && exitNDCDepth >= queryDepth) {
		return 2;   // cap intersection
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
	bool isFrontFace = true; // (exitNDCDepth > entryNDCDepth) != (queryDepth > lastQueryDepth);
	if (abs(lastQueryDepth - queryDepth) < continuousEpsilon && isFrontFace) {
		if (entryNDCDepth >= min(lastQueryDepth, queryDepth) && entryNDCDepth <= max(lastQueryDepth, queryDepth)) {
			return 1;
		}
	}

	return 0;
}

float4 MakeResult(PBI iterator, uint2 outputDimensions)
{
	if (!iterator._gotIntersection) return EmptyPixel;
	// return float4((iterator._testCount / 256.f).xxx, 1.0.x);		// (display step count)
	return BuildResult(
		iterator._distance,
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

	float4 result = EmptyPixel;

		//	when not doing the blur, the jittering actually looks better with
		//	a repeating pattern. For some reason, the regularity of it helps
		//	a completely random pattern just ends up looking messy and distracting
	float randomizerValue = GetRandomizerValue(dispatchThreadId.xy);

		// Iteration:
		//   0: new code
		//   1: reference high resolution iteration
		//   2: old code
	const uint stepMethod = 1;
	const float worldSpaceMaxDist = min(5.f, FarClip);
	if (stepMethod == 0) {

	} else if (stepMethod == 1) {
		ReflectionRay2 ray = CalculateReflectionRay2(worldSpaceMaxDist, dispatchThreadId.xy, outputDimensions, msaaSampleIndex);
		if (ray.valid) {
			PBISettings settings;
			settings.pixelStep = 8;
			settings.initialPixelsSkip = 2 + int(randomizerValue * (settings.pixelStep+1));
			PBI i = PixelBasedIteration(
				ViewToClipSpace(ray.viewStart), ViewToClipSpace(ray.viewEnd),
				float2(outputDimensions), settings);
			result = MakeResult(i, outputDimensions);
		}
	} else if (stepMethod == 2) {
		ReflectionRay ray = CalculateReflectionRay(dispatchThreadId.xy, outputDimensions, msaaSampleIndex);
		if (ray.valid) {
			result = MultiResolutionStep(
				ray.projStartPosition, ray.projBasicStep,
				randomizerValue, float2(outputDimensions), msaaSampleIndex);
		}
	}

	// result.rgb = 16.f * ray.basicStep.rgb;
	// result.a *= maskValue;		// we need this to pick up the fresnel calculation made in BuildMask.csh

	OutputTexture[dispatchThreadId.xy] = result;
	// OutputTexture[dispatchThreadId.xy] = float4(maskValue.xxx, 1.f);
}
