// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(REFLECTION_UTILITY_H)
#define REFLECTION_UTILITY_H

#include "../TransformAlgorithm.h"

// #define DEPTH_IN_LINEAR_COORDS
#define INTERPOLATE_IN_VIEW_SPACE

static const float DepthMinThreshold		= 0.f; //0.000001f;
static const float DepthMaxThreshold		= 1.f; // 0.01f;

static const uint DetailStepCount			= 12;
static const uint InitialStepCount			= 16;

static const uint MaskStepCount             = 8;
static const uint MaskSkipPixels            = 12;
static const float IteratingPower           = 2.5f;
static const uint TotalDistanceMaskShader	= MaskStepCount*MaskSkipPixels;

static const uint ReflectionDistancePixels  = 64u;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

cbuffer ViewProjectionParameters : register(b1)
{
	row_major float4x4	WorldToView;
	float3				ProjScale;
	float				ProjZOffset;
}

float3 CalculateWorldSpacePosition(uint2 samplingPixel, uint2 outputDimensions, uint msaaSampleIndex, out float outputLinearDepth)
{
    #if defined(DEPTH_IN_LINEAR_COORDS)
	    float linear0To1Depth = DownSampledDepth[samplingPixel];
    #else
        float linear0To1Depth = NDCDepthToLinear0To1(DownSampledDepth[samplingPixel]);
    #endif

 	float2 tc = samplingPixel / float2(outputDimensions);
 	float weight0 = (1.f - tc.x) * (1.f - tc.y);
 	float weight1 = (1.f - tc.x) * tc.y;
 	float weight2 = tc.x * (1.f - tc.y);
 	float weight3 = tc.x * tc.y;

 	float3 viewFrustumVector =
 			weight0 * FrustumCorners[0].xyz + weight1 * FrustumCorners[1].xyz
 		+   weight2 * FrustumCorners[2].xyz + weight3 * FrustumCorners[3].xyz
 		;

    outputLinearDepth = linear0To1Depth;
	return CalculateWorldPosition(viewFrustumVector, linear0To1Depth, WorldSpaceView);
}

float3 NDCToViewSpace(float4 ndc)
{
		// negate here because into the screen is -Z axis
	float Vz = -NDCDepthToWorldSpace(ndc.z);

		// undo projection -- assuming our projection matrix that
		// can fit into our "minimal projection" representation. If
		// there is an offset to XY part of the projection matrix, we
		// will need to apply that also...
	float2 projAdj = (ndc.xy * -Vz) / MinimalProjection.xy;
	return float3(projAdj, Vz);
}

float4 ViewToClipSpace(float3 viewSpace)
{
	return float4(
		MinimalProjection.x * viewSpace.x,
		MinimalProjection.y * viewSpace.y,
		MinimalProjection.z * viewSpace.z + MinimalProjection.w,
		-viewSpace.z);
}

float3 ViewToNDCSpace(float3 viewSpace)
{
	float4 clip = ViewToClipSpace(viewSpace);
	return clip.xyz/clip.w;
}

float3 ClipToViewSpace(float4 clipSpace)
{
	return float3(
		clipSpace.x / MinimalProjection.x,
		clipSpace.y / MinimalProjection.y,
		(clipSpace.z - MinimalProjection.w) / MinimalProjection.z);
}

float3 CalculateViewSpacePosition(uint2 samplingPixel, uint2 outputDimensions, uint msaaSampleIndex)
{
		// Screen coordinates -> view space is cheap if we make some
		// assumptions about the projection matrix!
		// undo viewport transform
	float2 preViewport = float2(
		-1.f + 2.0f * samplingPixel.x / float(outputDimensions.x),
		 1.f - 2.0f * samplingPixel.y / float(outputDimensions.y));

	return NDCToViewSpace(float4(preViewport, DownSampledDepth[samplingPixel], 1.f));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float2 AsZeroToOne(float2 ndc) { return float2(.5f + .5f * ndc.x, .5f - .5f * ndc.y); }

float LoadDepth(float2 ndcXY, float2 outputDimensions)
{
	return DownSampledDepth[uint2(AsZeroToOne(ndcXY)*outputDimensions.xy)];
}

float DepthDifference(float ndcDepth, float depthTextureValue)
{
	#if defined(DEPTH_IN_LINEAR_COORDS)
		return NDCDepthToLinear0To1(ndcDepth) - depthTextureValue;
	#else
		return ndcDepth - depthTextureValue;
	#endif
}

bool IsCollision(float ndcDepth, float depthTextureValue)
{
	float depthDifference = DepthDifference(ndcDepth, depthTextureValue);
	return depthDifference > DepthMinThreshold; // && depthDifference < DepthMaxThreshold;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool WithinClipCube(float4 clipSpacePosition)
{
	float3 tc = clipSpacePosition.xyz / clipSpacePosition.w;
	return max(max(abs(tc.x), abs(tc.y)), 1.f-tc.z) <= 1.f;
}

void ClipRay(float4 rayStart, inout float4 rayEnd)
{
		// clip x direction
	if ((rayEnd.x / rayEnd.w) < -1.f) {
		float4 rayVector = rayEnd - rayStart;
		float alpha = (-rayStart.x - rayStart.w) / (rayVector.x + rayVector.w);
		rayEnd = rayStart + alpha * rayVector;
	} else if ((rayEnd.x / rayEnd.w) > 1.f) {
		float4 rayVector = rayEnd - rayStart;
		float alpha = (rayStart.x - rayStart.w) / (rayVector.w - rayVector.x);
		rayEnd = rayStart + alpha * rayVector;
	}

		// clip y direction
	if ((rayEnd.y / rayEnd.w) < -1.f) {
		float4 rayVector = rayEnd - rayStart;
		float alpha = (-rayStart.y - rayStart.w) / (rayVector.y + rayVector.w);
		rayEnd = rayStart + alpha * rayVector;
	} else if ((rayEnd.y / rayEnd.w) > 1.f) {
		float4 rayVector = rayEnd - rayStart;
		float alpha = (rayStart.y - rayStart.w) / (rayVector.w - rayVector.y);
		rayEnd = rayStart + alpha * rayVector;
	}

		// clip z direction
	if ((rayEnd.z / rayEnd.w) < 0.f) {
		float4 rayVector = rayEnd - rayStart;
		float alpha0 = rayStart.z / -rayVector.z;
        float alpha1 = rayStart.w / -rayVector.w;
		rayEnd = rayStart + min(alpha0, alpha1) * rayVector;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ReflectionRay
{
	float4 projStartPosition;
	float4 projBasicStep;

    float2 screenSpaceRayDirection;

    float3 worldSpaceReflection;
    bool valid;
};

float3 LoadWorldSpaceNormal(uint2 pixelCoord)
{
	return DownSampledNormals[pixelCoord.xy].rgb;
}

float3 LoadViewSpaceNormal(uint2 pixelCoord)
{
		// hack -- 	we need a smarter way to get
		//			access to the view space normals
	return mul(
		(float3x3)WorldToView,
		DownSampledNormals[pixelCoord.xy].rgb);
}

ReflectionRay ReflectionRay_Invalid()
{
	ReflectionRay result;
	result.projStartPosition = result.projBasicStep = 0.0.xxxx;
	result.valid = false;
	return result;
}

float3 ClipViewSpaceRay(float3 viewStart, float3 viewEnd)
{
		// Given an input ray in view space; clip against the edge of the screen
		// We need to transform into homogeneous clip space to do the clipping
	float4 clipStart = ViewToClipSpace(viewStart);
	float4 clipEnd = ViewToClipSpace(viewEnd);

	if (clipEnd.x > clipEnd.w) {
		float l = (clipStart.w - clipStart.x) / ((clipStart.w - clipStart.x) - (clipEnd.w - clipEnd.x));
		clipEnd = lerp(clipStart, clipEnd, l);
	}

	if (clipEnd.y > clipEnd.w) {
		float l = (clipStart.w - clipStart.y) / ((clipStart.w - clipStart.y) - (clipEnd.w - clipEnd.y));
		clipEnd = lerp(clipStart, clipEnd, l);
	}

	if (clipEnd.x < -clipEnd.w) {
		float l = (clipStart.w - -clipStart.x) / ((clipStart.w - -clipStart.x) - (clipEnd.w - -clipEnd.x));
		clipEnd = lerp(clipStart, clipEnd, l);
	}

	if (clipEnd.y < -clipEnd.w) {
		float l = (clipStart.w - -clipStart.y) / ((clipStart.w - -clipStart.y) - (clipEnd.w - -clipEnd.y));
		clipEnd = lerp(clipStart, clipEnd, l);
	}

	return ClipToViewSpace(clipEnd);
}

struct ReflectionRay2
{
	float3 viewStart;
	float3 viewEnd;
	bool valid;
};

ReflectionRay2 ReflectionRay2_Invalid()
{
	ReflectionRay2 result;
	result.valid = false;
	return result;
}

float3 GetTestPt(ReflectionRay2 ray, uint index, float randomizerValue)
{
		// note that we're returning points that are linear in view space
		// this might not be as ideal as points that are linear in screen
		// space... But the difference seems to be minor so long as we're
		// skipping over pixels each time.
		// It might be ideal to have more samples close the start...?
	return lerp(ray.viewStart, ray.viewEnd, (index+.25f+.75f*randomizerValue) / 8.f);
}

float3 GetTestPtNDC(ReflectionRay2 ray, uint index, float randomizerValue)
{
	return ViewToNDCSpace(GetTestPt(ray, index, randomizerValue));
}

uint GetTestPtCount(ReflectionRay2 ray) { return 8; }

ReflectionRay2 CalculateReflectionRay2(uint2 pixelCoord, uint2 outputDimensions, uint msaaSampleIndex)
{
	float3 rayStartView = CalculateViewSpacePosition(pixelCoord.xy, outputDimensions, msaaSampleIndex);
	float3 viewSpaceNormal = LoadViewSpaceNormal(pixelCoord);
	if (dot(viewSpaceNormal, viewSpaceNormal) < 0.25f)
		return ReflectionRay2_Invalid();
	float3 reflection = reflect(normalize(rayStartView), viewSpaceNormal);

		// We want to find the point where the reflection vector intersects the edge of the
		// view frustum. We can do this just by transforming the ray into NDC coordinates,
		// and doing a clip against the +-W box.
	float maxReflectionDistanceWorldSpace = min(5.f, FarClip);
	float3 rayEndView = rayStartView + maxReflectionDistanceWorldSpace * reflection;
	rayEndView = ClipViewSpaceRay(rayStartView, rayEndView);

	ReflectionRay2 result;
	result.valid = true;
	result.viewStart = rayStartView;
	result.viewEnd = rayEndView;
	return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ReflectionRay CalculateReflectionRay(uint2 pixelCoord, uint2 outputDimensions, uint msaaSampleIndex)
{
    float linearDepth;
	float3 worldSpacePosition =
        CalculateWorldSpacePosition(
		    pixelCoord.xy, outputDimensions, msaaSampleIndex, linearDepth);

			// Normals are stored in basic floating point format in the downsampled normals texture
	float3 worldSpaceNormal			= DownSampledNormals[pixelCoord.xy].rgb;
 	float3 worldSpaceReflection		= reflect(worldSpacePosition - WorldSpaceView, worldSpaceNormal);

    if (dot(worldSpaceNormal, worldSpaceNormal) < 0.25f)
		return ReflectionRay_Invalid();

		// todo --	this imaginary distance affects the pixel step rate too much.
		//			we could get a better imaginary distance by just calculating the distance
		//			to the edge of the screen.
	const float imaginaryDistance	= .05f;
	float3 worldSpaceRayEnd			= worldSpacePosition + imaginaryDistance * normalize(worldSpaceReflection);
    float4 projectedRayStart		= mul(WorldToClip, float4(worldSpacePosition, 1.f));

    ReflectionRay result;
    #if !defined(INTERPOLATE_IN_VIEW_SPACE)
	    float4 projectedRayEnd			= mul(WorldToClip, float4(worldSpaceRayEnd, 1.f));
	    float4 projectedDiff			= projectedRayEnd - projectedRayStart;
	    float4 basicStep				= projectedDiff;

	    const bool adaptiveLength = true;
	    if (adaptiveLength) {
		    float aveW = abs(lerp(projectedRayStart.w, projectedRayEnd.w, 0.5f));	// if w values are similar across ray, we can estimate pixel step size
		    float2 basicStepScaleXY;
		    basicStepScaleXY.x = 1.f / abs(projectedDiff.x / aveW * 0.5f * outputDimensions.x);
		    basicStepScaleXY.y = 1.f / abs(projectedDiff.y / aveW * 0.5f * outputDimensions.y);
		    // basicStep *= 32.f * max(min(basicStepScaleXY.x, basicStepScaleXY.y), 1.0f/(512.f*512.f));
            // basicStep *= 32.f * min(basicStepScaleXY.x, basicStepScaleXY.y);
            basicStep *= min(basicStepScaleXY.x, basicStepScaleXY.y);
	    } else {
		    basicStep = (.01f / length(basicStep.xy)) * basicStep;
	    }

	    result.projStartPosition = projectedRayStart; // + 2.5f * basicStep;
	    result.projBasicStep = basicStep;
    #else
		float3 viewSpaceRayStart = CalculateViewSpacePosition(pixelCoord.xy, outputDimensions, msaaSampleIndex);
        // float3 viewSpaceRayStart = mul(WorldToView, float4(worldSpacePosition, 1.f)).xyz;

	    float3 viewSpaceRayEnd	 = mul(WorldToView, float4(worldSpaceRayEnd, 1.f)).xyz;
	    float3 viewSpaceDiff	 = viewSpaceRayEnd - viewSpaceRayStart;
	    float4 basicStep		 = float4(viewSpaceDiff, 1.f);

            // (    because it's view space, we can't really know how many pixels are between these points. We want ideally
            //      we want to step on pixel at a time, but that requires some more math to figure out the number of
            //      pixels between start and end, and the calculate the location of each pixel)
		// basicStep /= 256.f;
        // basicStep = (.01f / length(basicStep.xy)) * basicStep;
        basicStep = (.1f / length(basicStep.xy)) * basicStep;
        // basicStep = normalize(basicStep) / 1024.f;

	    result.projStartPosition = float4(viewSpaceRayStart, 1);
	    result.projBasicStep = basicStep;
    #endif

        //      Any straight line in world space should become a straight line in projected space
        //      (assuming the line is entirely within the view frustum). So, if we find 2 points
        //      on this ray (that are within the view frustum) and project them, we should be able
        //      to calculate the gradient of the line in projected space.
        //      The start point must be within the view frustum, because we're starting from a pixel
        //      on the screen.

    {
        float3 imaginaryEnd = worldSpacePosition + 1.f * normalize(worldSpaceReflection);
	    float4 projectedRayEnd	 = mul(WorldToClip, float4(imaginaryEnd, 1.f));
        ClipRay(projectedRayStart, projectedRayEnd);    // (make sure we haven't exited the view frustum)
        result.screenSpaceRayDirection = normalize(
            (projectedRayEnd.xy / projectedRayEnd.w) - (projectedRayStart.xy / projectedRayStart.w));

        // result.projStartPosition.z += 0.01f;
    }

    result.valid = linearDepth != 1.f; // linearDepth < 0.995f;        // we get strange results with unfilled areas of the depth buffer. So, if depth is too far, it's invalid
    result.worldSpaceReflection = worldSpaceReflection;

	return result;
}

float4 IteratingPositionToClipSpace(float4 testPosition)
{
    #if defined(INTERPOLATE_IN_VIEW_SPACE)
		return ViewToClipSpace(testPosition.xyz);
	#else
		return testPosition;
	#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float GetRandomizerValue(uint2 dispatchThreadId)
{
	int ditherArray[16] =
	{
		4, 12,  0,  8,
		10,  2, 14,  6,
		15,  7, 11,  3,
		1,  9,  5, 13
	};
	uint2 t = dispatchThreadId.xy & 0x3;
	return float(ditherArray[t.x+t.y*4]) / 15.f;
}

#endif
