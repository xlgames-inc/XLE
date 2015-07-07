// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(DEBUGGING_SHAPES_H)
#define DEBUGGING_SHAPES_H

#include "../CommonResources.h"
#include "EdgeDetection.h"

Texture2D				RefractionsBuffer : register(t12);

static const float SqrtHalf = 0.70710678f;
static const float3 BasicShapesLightDirection = normalize(float3(SqrtHalf, SqrtHalf, -0.25f));

struct DebuggingShapesCoords
{
    float aspectRatio;
    float2 outputDimensions;
    float2 refractionCoords;
    float2 texCoord;
    float2 udds;
    float2 vdds;
};

DebuggingShapesCoords DebuggingShapesCoords_Make(float4 position, float2 texCoord, float2 outputDimensions)
{
    DebuggingShapesCoords result;
    // result.aspectRatio = outputDimensions.y/outputDimensions.x;
    result.outputDimensions = outputDimensions;
    result.refractionCoords = position.xy/outputDimensions.xy;
    result.texCoord = texCoord;

    result.udds = float2(ddx(texCoord.x), ddy(texCoord.x));
    result.vdds = float2(ddx(texCoord.y), ddy(texCoord.y));

        //  We can calculate the aspect ratio of tex coordinate mapping
        //  by looking at the screen space derivatives
    float texCoordAspect = length(result.vdds)/length(result.udds);
    result.aspectRatio = 1.f/texCoordAspect;

    return result;
}

float TagShape(float2 minCoords, float2 maxCoords, float2 texCoord, float aspectRatio)
{
	const float roundedProportion = 0.4f;
	float roundedHeight = (maxCoords.y - minCoords.y) * roundedProportion;
	if (	texCoord.x < minCoords.x || texCoord.x > maxCoords.x
		||	texCoord.y < minCoords.y || texCoord.y > maxCoords.y) {
		return 0.f;
	}

	float roundedWidth = roundedHeight * aspectRatio;

	float2 r = texCoord - minCoords;
	if (r.x < roundedWidth) {

		if (r.y < roundedHeight) {
			float2 centre = float2(roundedWidth, roundedHeight);
			float2 o = r - centre; o.x /= aspectRatio;
			return dot(o, o) <= (roundedHeight*roundedHeight);
		} else if (r.y > maxCoords.y - minCoords.y - roundedHeight) {
			float2 centre = float2(roundedWidth, maxCoords.y - minCoords.y - roundedHeight);
			float2 o = r - centre; o.x /= aspectRatio;
			return dot(o, o) <= (roundedHeight*roundedHeight);
		} else {
			return 1.f;
		}

	} else {

		float sliceWidth = (maxCoords.y - minCoords.y) * .5f * aspectRatio;
		float sliceStart = maxCoords.x - minCoords.x - sliceWidth;
		if (r.x > sliceStart) {
			float a = (r.x - sliceStart) / sliceWidth;
			if (r.y < (1.f-a) * (maxCoords.y - minCoords.y)) {
				return 1.f;
			}
		} else {
			return 1.f;
		}
	}

	return 0.f;
}

void RenderTag(float2 minCoords, float2 maxCoords, DebuggingShapesCoords coords, inout float4 result)
{
		//
		//		Using "sharr" filter to find image gradient. We can use
		//		this to create a 3D effect for any basic shape.
		//		See:
		//			http://www.hlevkin.com/articles/SobelScharrGradients5x5.pdf
		//

    const float border = 1.f;
    float2 tagMin = minCoords + border * coords.udds + border * coords.vdds;
    float2 tagMax = maxCoords - border * coords.vdds - border * coords.vdds;

	float2 dhdp = 0.0.xx;
	for (uint y=0; y<5; ++y) {
		for (uint x=0; x<5; ++x) {
			float2 texCoordOffset = ((x-2.f) * coords.udds) + ((y-2.f) * coords.vdds);
			float t = TagShape(tagMin, tagMax, coords.texCoord + texCoordOffset, coords.aspectRatio);
			dhdp.x += SharrHoriz5x5[x][y] * t;
			dhdp.y += SharrVert5x5[x][y] * t;
		}
	}

	// result = float4(0.5f + 0.5f * dhdp.xy, 0.1, 1.f);

	float t = TagShape(tagMin, tagMax, coords.texCoord, coords.aspectRatio);
	if (t > 0.f) {
		float3 u = float3(1.f, 0.f, dhdp.x);
		float3 v = float3(0.f, 1.f, dhdp.y);
		float3 normal = normalize(cross(v, u));

		float d = saturate(dot(BasicShapesLightDirection, normal));
		float A = 7.5f * pow(d, 2.f);

		result = float4(A * 2.f * float3(0.125f, 0.2f, .25f) + 0.1.xxx, 1.f);

        result.rgb += RefractionsBuffer.SampleLevel(ClampingSampler, coords.refractionCoords, 0).rgb;

	} else {
		float b = max(abs(dhdp.x), abs(dhdp.y));
		const float borderSize = .125f;
		if (b >= borderSize) {
			result = float4(0.0.xxx, b/borderSize);
		}
	}
}

float RoundedRectShape(
    float2 minCoords, float2 maxCoords, float2 texCoord,
    float aspectRatio, float roundedProportion)
{
	if (	texCoord.x < minCoords.x || texCoord.x > maxCoords.x
		||	texCoord.y < minCoords.y || texCoord.y > maxCoords.y) {
		return 0.f;
	}

	float roundedHeight = (maxCoords.y - minCoords.y) * roundedProportion;
    float roundedWidth = roundedHeight * aspectRatio;

        // mirror coords so we only have to consider the top/left quadrant
    float2 r = float2(
        min(maxCoords.x - texCoord.x, texCoord.x) - minCoords.x,
        min(maxCoords.y - texCoord.y, texCoord.y) - minCoords.y);

	if (r.x < roundedWidth && r.y < roundedHeight) {
		float2 centre = float2(roundedWidth, roundedHeight);
		float2 o = r - centre; o.x /= aspectRatio;
		return dot(o, o) <= (roundedHeight*roundedHeight);
	}
	return 1.f;
}

float2 RoundedRectShape2(
    float2 minCoords, float2 maxCoords,
    DebuggingShapesCoords coords, float borderSizePix, float roundedProportion)
{
    float2 texCoord = coords.texCoord;
	if (	texCoord.x < minCoords.x || texCoord.x > maxCoords.x
		||	texCoord.y < minCoords.y || texCoord.y > maxCoords.y) {
		return 0.0.xx;
	}

    float2 pixelSize = float2(coords.udds.x, coords.vdds.y);
    float2 borderSize = borderSizePix * pixelSize;

	float roundedHeight = (maxCoords.y - minCoords.y) * roundedProportion;
    float roundedWidth = roundedHeight * coords.aspectRatio;

        // mirror coords so we only have to consider the top/left quadrant
    float2 r = float2(
        min(maxCoords.x - texCoord.x, texCoord.x) - minCoords.x,
        min(maxCoords.y - texCoord.y, texCoord.y) - minCoords.y);

	if (r.x < roundedWidth && r.y < roundedHeight) {
		float2 centre = float2(roundedWidth, roundedHeight);

        ////////////////
            //  To get a anti-aliased look to the edges, we need to make
            //  several samples. Lets just use a simple pattern aligned
            //  to the pixel edges...
        float2 samplePts[4] =
        {
            float2(.5f, .2f), float2(.5f, .8f),
            float2(.2f, .5f), float2(.8f, .5f),
        };

        float2 result = 0.0.xx;
        for (uint c=0; c<4; ++c) {
		    float2 o = r - centre + samplePts[c] * pixelSize;
            o.x /= coords.aspectRatio;
            float dist = roundedHeight - length(o);
            result.x += .25f * (dist >= 0.f && dist < borderSize.y);
            result.y = max(result.y, dist >= borderSize.y);
        }
        return result;
	}
    if (r.x < borderSize.x || r.y < borderSize.y) {
        return 1.0.xx;
    }

	return float2(0.f, 1.f);
}

float CircleShape(float2 centrePoint, float radius, float2 texCoord, float aspectRatio)
{
	float2 o = texCoord - centrePoint;
	o.x /= aspectRatio;
	return dot(o, o) <= (radius*radius);
}

float RectShape(float2 minCoords, float2 maxCoords, float2 texCoord)
{
	return texCoord.x >= minCoords.x && texCoord.x < maxCoords.x
		&& texCoord.y >= minCoords.y && texCoord.y < maxCoords.y;
}

float ScrollBarShape(float2 minCoords, float2 maxCoords, float thumbPosition, float2 texCoord, float aspectRatio)
{
	float2 baseLineMin = float2(minCoords.x, lerp(minCoords.y, maxCoords.y, 0.4f));
	float2 baseLineMax = float2(maxCoords.x, lerp(minCoords.y, maxCoords.y, 0.6f));
	float result = 0.5f * RoundedRectShape(baseLineMin, baseLineMax, texCoord, aspectRatio, 0.4f);

		//	Add small markers at fractional positions along the scroll bar
	float markerPositions[7] = { .125f, .25f, .375f, .5f,   .625f, .75f, .875f };
	float markerHeights[7]   = { .5f  , .75f, .5f ,  .825f, .5f,   .75f, .5f   };

	for (uint c=0; c<7; ++c) {
		float x = lerp(minCoords.x, maxCoords.x, markerPositions[c]);
		float2 markerMin = float2(x - 0.002f, lerp(minCoords.y, maxCoords.y, 0.5f*(1.f-markerHeights[c])));
		float2 markerMax = float2(x + 0.002f, lerp(minCoords.y, maxCoords.y, 0.5f+0.5f*markerHeights[c]));
		result = max(result, 0.75f*RectShape(markerMin, markerMax, texCoord));
	}

	float2 thumbCenter = float2(
		lerp(minCoords.x, maxCoords.x, thumbPosition),
		lerp(minCoords.y, maxCoords.y, 0.5f));
	result = max(result, CircleShape(thumbCenter, 0.475f * (maxCoords.y - minCoords.y), texCoord, aspectRatio));
	return result;
}

void RenderScrollBar(   float2 minCoords, float2 maxCoords, float thumbPosition,
                        DebuggingShapesCoords coords, inout float4 result)
{


	float2 dhdp = 0.0.xx;
	for (uint y=0; y<5; ++y) {
		for (uint x=0; x<5; ++x) {
			    //  Note that we want the sharr offsets to be in units of screen space pixels
                //  So, we can use screen space derivatives to calculate the correct offsets
                //  in texture coordinate space
            float2 texCoordOffset = ((x-2.f) * coords.udds) + ((y-2.f) * coords.vdds);
			float t = ScrollBarShape(minCoords, maxCoords, thumbPosition, coords.texCoord + texCoordOffset, coords.aspectRatio);
			dhdp.x += SharrHoriz5x5[x][y] * t;
			dhdp.y += SharrVert5x5[x][y] * t;
		}
	}

	float t = ScrollBarShape(minCoords, maxCoords, thumbPosition, coords.texCoord, coords.aspectRatio);
	if (t > 0.f) {
		float3 u = float3(1.f, 0.f, dhdp.x);
		float3 v = float3(0.f, 1.f, dhdp.y);
		float3 normal = normalize(cross(v, u));

		float d = saturate(dot(BasicShapesLightDirection, normal));
		float A = 12.5f * pow(d, 2.f);

		if (t > 0.75f) {
			result = float4(A * 2.f * float3(0.125f, 0.2f, .25f) + 0.1.xxx, 1.f);
            result.rgb += RefractionsBuffer.SampleLevel(ClampingSampler, coords.refractionCoords, 0).rgb;
		} else {
			// result = float4(A * float3(0.125f, 0.1f, .1f) + 0.1.xxx, 1.f);
            result = float4(A * float3(1.1f, .9f, .5f) + 0.1.xxx, 1.f);
            result.rgb += 0.5f * RefractionsBuffer.SampleLevel(ClampingSampler, coords.refractionCoords, 0).rgb;
		}


	} else {
        float b = max(abs(dhdp.x), abs(dhdp.y));
		const float borderSize = .125f;
		if (b >= borderSize) {
			result = float4(0.0.xxx, b/borderSize);
		}
	}
}

#endif
