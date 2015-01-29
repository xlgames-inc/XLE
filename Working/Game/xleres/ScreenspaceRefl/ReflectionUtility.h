// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(REFLECTION_UTILITY_H)
#define REFLECTION_UTILITY_H

#include "../TransformAlgorithm.h"

// #define DEPTH_IN_LINEAR_COORDS
// #define INTERPOLATE_IN_VIEW_SPACE

static const float DepthMinThreshold		= 0.000001f;
static const float DepthMaxThreshold		= 0.01f;

static const uint DetailStepCount			= 12;
static const uint InitialStepCount			= 16;

static const uint MaskStepCount             = 16;
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
	    float linearDepth = DepthTexture[samplingPixel]; // LoadFloat1(DepthTexture, samplingPixel, msaaSampleIndex);
    #else
        float linearDepth = NDCDepthToLinearDepth(DepthTexture[samplingPixel]);
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
 
    outputLinearDepth = linearDepth;
	return CalculateWorldPosition(viewFrustumVector, linearDepth, NearClip, FarClip, WorldSpaceView);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float2 AsTexCoord(float2 ndc) { return float2(.5f + .5f * ndc.x, .5f - .5f * ndc.y); }

float CalculateDepthDifference(float4 position, float2 outputDimensions)
{
    float2 tc = AsTexCoord(position.xy / position.w);
    #if defined(DEPTH_IN_LINEAR_COORDS)
	    float queryLinearDepth	= DepthTexture[uint2(tc.xy*outputDimensions.xy)];
 	    float depthDifference	= NDCDepthToLinearDepth(position.z / position.w) - queryLinearDepth;
    #else
        float queryDepth = DepthTexture[uint2(tc.xy*outputDimensions.xy)];
        float depthDifference = (position.z/position.w) - queryDepth;
    #endif
    return depthDifference;
}

bool CompareDepth(float4 position, float2 outputDimensions)
{
	float depthDifference = CalculateDepthDifference(position, outputDimensions);
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

ReflectionRay CalculateReflectionRay(uint2 pixelCoord, uint2 outputDimensions, uint msaaSampleIndex)
{
    float linearDepth;
	float3 worldSpacePosition = 
        CalculateWorldSpacePosition(
		    pixelCoord.xy, outputDimensions, msaaSampleIndex, linearDepth);

			// normals are stored in basic floating point format in the downsampled normals
			// texture (not UNORM format, ala the main normals buffer)
	float3 worldSpaceNormal			= DownSampledNormals[pixelCoord.xy].rgb;
 	float3 worldSpaceReflection		= reflect(worldSpacePosition - WorldSpaceView, worldSpaceNormal);

    if (dot(worldSpaceNormal, worldSpaceNormal) < 0.25f) {
        ReflectionRay result;
        result.projStartPosition = result.projBasicStep = 0.0.xxxx;
        result.valid = false;
	    return result;
    }

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
        float4 viewSpaceRayStart = mul(WorldToView, float4(worldSpacePosition, 1.f));
	    float4 viewSpaceRayEnd	 = mul(WorldToView, float4(worldSpaceRayEnd, 1.f));
	    float4 viewSpaceDiff	 = viewSpaceRayEnd - viewSpaceRayStart;
	    float4 basicStep		 = viewSpaceDiff; 

            // (    because it's view space, we can't really know how many pixels are between these points. We want ideally
            //      we want to step on pixel at a time, but that requires some more math to figure out the number of
            //      pixels between start and end, and the calculate the location of each pixel)
		// basicStep /= 256.f;      
        // basicStep = (.01f / length(basicStep.xy)) * basicStep;
        basicStep = (.1f / length(basicStep.xy)) * basicStep;
        // basicStep = normalize(basicStep) / 1024.f;      

	    result.projStartPosition = viewSpaceRayStart;
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

    result.valid = linearDepth < 0.995f;        // we get strange results with unfilled areas of the depth buffer. So, if depth is too far, it's invalid
    result.worldSpaceReflection = worldSpaceReflection;

	return result;
}

float4 IteratingPositionToProjSpace(float4 testPosition)
{
    #if defined(INTERPOLATE_IN_VIEW_SPACE)
		return float4(
			ProjScale.x * testPosition.x,
			ProjScale.y * testPosition.y,
			ProjScale.z * testPosition.z + ProjZOffset,
			-testPosition.z);
	#else
		return testPosition;
	#endif
}

#endif
