// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../TransformAlgorithm.h"
#include "../CommonResources.h"
#include "../ShadowProjection.h"
#include "../Lighting/BasicLightingEnvironment.h"
#include "../Utility/perlinnoise.h"
#include "../Utility/MathConstants.h"
#include "../Colour.h"
#include "VolumetricFog.h"

Texture2DArray<float>		ShadowTextures	 	: register(t2);

RWTexture3D<float>			InscatterShadowingValues	: register(u0);
RWTexture3D<float>			TransmissionValues			: register(u1);
RWTexture3D<float>			DensityValues				: register(u2);
RWTexture3D<float4>			InscatterOutput				: register(u3);

Texture3D<float>			InputInscatterShadowingValues	: register(t3);
Texture3D<float4>			InputInscatterPointLightSources : register(t4);
Texture2D<float>			NoiseValues						: register(t9);

static const float WorldSpaceGridDepth = 150.f;
static const int3 GridDimensions = int3(160, 90, 128);

	//
	//		 -- todo -- how to rotate the grid? perhaps increase in depth should be
	//					sequential in memory -- so pushing light forward through the
	//					grid is quicker
	//

float ResolveShadows(float3 worldPosition)
{
	const bool shadowsPerspectiveProj = ShadowsPerspectiveProjection;

	float result = 1.f;
		// note -- we might not really need every shadow projection here. Just the first 2 or 3 cascades might be enough
	int projectionCount = GetShadowSubProjectionCount();
	for (int c=0; c<projectionCount; ++c) {
		float4 frustumCoordinates = ShadowProjection_GetOutput(worldPosition, c);

		float d = frustumCoordinates.z / frustumCoordinates.w;
		float2 texCoords = frustumCoordinates.xy / frustumCoordinates.w;

		if (	(max(abs(texCoords.x), abs(texCoords.y)) < 1.f)
			&&	(max(d, 1.f-d) < 1.f)) {
			texCoords = float2(0.5f + 0.5f * texCoords.x, 0.5f - 0.5f * texCoords.y);

			float esmSample = ShadowTextures.SampleLevel(DefaultSampler, float3(texCoords, float(c)), 0);
			float linearComparisonDistance;
			float4 miniProj = ShadowProjection_GetMiniProj(c);
			if (shadowsPerspectiveProj) {
				linearComparisonDistance = NDCDepthToWorldSpace_Perspective(d, AsMiniProjZW(miniProj));
			} else {
				linearComparisonDistance = NDCDepthToWorldSpace_Ortho(d, AsMiniProjZW(miniProj));
			}

			#if ESM_SHADOW_MAPS==1
						//	As per esm resolve equations...
				return saturate(exp(-ESM_C*(linearComparisonDistance + ShadowsBias)) * esmSample);
			#else
				return linearComparisonDistance < esmSample;
			#endif
		}

	}

	return result;
}

float3 CalculateSamplePoint(uint3 cellIndex)
{
	float3 multiplier = 1.0.xxx / float3(GridDimensions);
	float2 centralNearPlaneXYCoords = float2(
		(float2(cellIndex.xy) + 0.5.xx) * multiplier.xy);
	centralNearPlaneXYCoords.y = 1.0f - centralNearPlaneXYCoords.y;

	float gridCellCentreDepth	= (cellIndex.z + 0.5) * multiplier.z;
	// gridCellCentreDepth *= gridCellCentreDepth;
	gridCellCentreDepth *= WorldSpaceGridDepth;

		//
		//		Jittering the sampling position helps get
		//		a higher resolution result. The end result just looks
		//		much smoother.
		//
		//			we could also do a single noise lookup, and then
		//			use the noise result to look up a table of 3d offsets.
		//
	#define JITTER_SAMPLE_POSITION
	#if defined(JITTER_SAMPLE_POSITION)
			// note --	would we be better off with a more regular pattern for this jittering?
			//			currently using just random inputs.
		float noise0 = -1.f + 2.f * NoiseValues[int2(cellIndex.x, cellIndex.y*cellIndex.z) & 0xff];
		float noise1 = -1.f + 2.f * NoiseValues[int2(cellIndex.y*cellIndex.x, cellIndex.z) & 0xff];
		float noise2 = NoiseValues[int2(cellIndex.z, cellIndex.x*cellIndex.y) & 0xff];

		centralNearPlaneXYCoords.x += JitteringAmount * multiplier.x * noise0;
		centralNearPlaneXYCoords.y += JitteringAmount * multiplier.y * noise1;
		gridCellCentreDepth += WorldSpaceGridDepth * JitteringAmount * multiplier.z * noise2;
	#endif

	float topLeftWeight		= (1.f - centralNearPlaneXYCoords.x) * centralNearPlaneXYCoords.y;
	float bottomLeftWeight	= (1.f - centralNearPlaneXYCoords.x) * (1.f - centralNearPlaneXYCoords.y);
	float topRightWeight	= centralNearPlaneXYCoords.x * centralNearPlaneXYCoords.y;
	float bottomRightWeight = centralNearPlaneXYCoords.x * (1.f - centralNearPlaneXYCoords.y);

	float3 viewFrustumVectorToCentre =
		  topLeftWeight		* FrustumCorners[0].xyz
		+ bottomLeftWeight	* FrustumCorners[1].xyz
		+ topRightWeight	* FrustumCorners[2].xyz
		+ bottomRightWeight * FrustumCorners[3].xyz
		;

	return CalculateWorldPosition(
		viewFrustumVectorToCentre, gridCellCentreDepth / FarClip,
		WorldSpaceView);
}

float MonochromeRaleighScattering(float cosTheta)
{
	return 3.f / (16.f * pi) * (1.f + cosTheta * cosTheta);
}

[numthreads(10, 10, 8)]
	void InjectLighting(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	float3 centrePoint = CalculateSamplePoint(dispatchThreadId);

		//	Jittering the centre point gives the fog
		//	a nice natural look... and helps blur the edge of shadows

			//	todo -- too many noise samples. Use a precalculated 3d texture

	const bool closeSample = dispatchThreadId.z < 16;
	float noiseSample1 = 0.f;

	// #define DO_NOISE_OFFSET
	#if DO_NOISE_OFFSET==1
		float t = Time * .2f * NoiseSpeed;
		if (closeSample) {
			centrePoint += 1.f * (PerlinNoise3D(centrePoint.xyz + t * 0.47f * float3(2.13f, 1.771f, 2.2422f)) - 0.5f);
		}
		noiseSample1 = PerlinNoise3D(0.0525f * centrePoint.xyz + t * 1.f * float3(1.13f, 0.771f, 1.2422f));
	#endif

		//	grid cell volume is determined by field of view, aspect ratio
		//	and cell depth value

		//	Calculate shadows at this world coordinate

	float shadowingSample = ResolveShadows(centrePoint);

		//	Colour & quantity of inscatter should be dependant on the angle between
		//	the light and the view direction

		//	Write final result to the volume texture

	int3 outputTexel = dispatchThreadId;
	const float convenienceScalar = 0.1f;	// scalar just to make "Density" valid between 0 and 1
	float density = max(0, convenienceScalar * (Density + NoiseDensityScale * noiseSample1));

		// just linear with height currently...
	float heightDensityScale = saturate((centrePoint.z - HeightStart) / (HeightEnd - HeightStart));
	density *= heightDensityScale;

	InscatterShadowingValues[outputTexel] = shadowingSample;
	DensityValues[outputTexel] = density;
}

static float3 GetDirectionToSun()
{
	return -normalize(BasicLight[0].NegativeDirection);
}

float3 CalculateInscatter(int3 dispatchThreadId, float density)
{
	float3 centrePoint = CalculateSamplePoint(dispatchThreadId);
	float3 directionToSun = GetDirectionToSun();
	float3 directionToSample = centrePoint - WorldSpaceView;
	float directionToSampleRLength = rsqrt(dot(directionToSample, directionToSample));
	float cosTheta = dot(directionToSun, directionToSample) * directionToSampleRLength;

			//
			//		Smooth the search through the shadowing grid
			//		by jittering X & Y slightly as we walk through
			//		the grid.
			//
	#define SMOOTH_SHADOWING_RESULT
	#if defined(SMOOTH_SHADOWING_RESULT)
		float2 offset;
		offset.x = frac(0.5f * 0.125f * dispatchThreadId.z);
		offset.y = frac(0.5f * 0.178f * dispatchThreadId.z);
		uint3 dimensions; InputInscatterShadowingValues.GetDimensions(dimensions.x, dimensions.y, dimensions.z);
		float3 texCoord = (dispatchThreadId + float3(offset, 0.5f)) / float3(dimensions);
		// float3 texCoord = float3(dispatchThreadId.xyz + 0.5.xxx) / float3(dimensions);
		float shadowing = InputInscatterShadowingValues.SampleLevel(DefaultSampler, texCoord, 0);
	#else
		float shadowing = InputInscatterShadowingValues[dispatchThreadId];
	#endif

	float4 inscatterPointLightSources = InputInscatterPointLightSources[dispatchThreadId];
	float raleighScattering	 = MonochromeRaleighScattering(cosTheta);
	float inscatterScalar = shadowing * raleighScattering * density;		// inscatter quantity must be scale with the volume of the cell

	// const float3 forwardColour = 17.f * float3(.7, .6f, 1.f);
	// const float3 backColour = 17.f * float3(0.5f, 0.5f, .65f);

	float3 colour = lerp(ForwardColour, BackColour, 0.5f + 0.5f * cosTheta);
	return (inscatterScalar * colour) + inscatterPointLightSources.rgb;
}

[numthreads(10, 10, 1)]
	void PropagateLighting(uint3 dispatchThreadId : SV_DispatchThreadID)
{
		// \todo -- Use a shader resource and write to a second uva or read
		//			and write to the same UVA? We can only read from single
		//			element UVAs.
		//			We could use an int type that is packed RGBA?

	float frontDensity = DensityValues[int3(dispatchThreadId.xy, 0)];
	float4 accumulatedInscatter = float4(CalculateInscatter(int3(dispatchThreadId.xy, 0), frontDensity), 1.f);
	float accumulatedTransmission = 1.f;
	TransmissionValues[int3(dispatchThreadId.xy, 0)] = accumulatedTransmission;
	InscatterOutput[int3(dispatchThreadId.xy, 0)] = 0.0.xxxx;

	for (int d=1; d<=127; d++) {

		float backDensity = DensityValues[int3(dispatchThreadId.xy, d)];

			//	Note, each cell is progressively larger towards the camera, but the z-dimension
			//	of the cells are the same.
			//		-- but should this affect the simulation in any way?
			//			In effect, we're simulating a single ray... So does the volume really matter?
		const float transmissionDistance = WorldSpaceGridDepth / float(GridDimensions.z);

		float3 inscatter		 = CalculateInscatter(int3(dispatchThreadId.xy, d), backDensity);
		float4 newInscatter		 = float4(inscatter, 1.f);

			//	using Beer-Lambert equation for fog outscatter
		float integralSolution	 = (backDensity + frontDensity) * .5f * transmissionDistance;
		float transmissionScalar = exp(-integralSolution);

		accumulatedInscatter	 = accumulatedInscatter + newInscatter * accumulatedTransmission;
		accumulatedTransmission *= transmissionScalar;

			// accumulatedValue = min(accumulatedValue, newSample);
			// accumulatedValue = max(accumulatedValue, newSample);

		InscatterOutput   [int3(dispatchThreadId.xy, d)] = accumulatedInscatter;
		TransmissionValues[int3(dispatchThreadId.xy, d)] = accumulatedTransmission;

		frontDensity = backDensity;

	}
}

//////////////////////////

	struct Light
	{
		float3		WorldSpacePosition;
		float		Radius;
		float3		Colour;
		float		Power;
	};

	struct ProjectedLight
	{
		float3		ViewSpacePosition;
		float		ViewSpaceHalfSubtendingAngle;
		float2		BaseAngles;
	};

	StructuredBuffer<Light>				InputLightList			: register(t0);
	RWStructuredBuffer<ProjectedLight>	ProjectedLightList		: register(u1);

	cbuffer LightCulling : register(b2)
	{
		int					LightCount;
		int2				GroupCounts;
		row_major float4x4	WorldToView;
		float2				FOV;
	}

	Light GetInputLight(uint lightIndex)				{ return InputLightList[lightIndex]; }
	ProjectedLight GetProjectedLight(uint lightIndex)	{ return ProjectedLightList[lightIndex]; }

//////////////////////////

Texture2D<float>				AirLightLookup			: register(t1);

float DistanceAttenuation(float distanceSq, float power)
{
	float attenuation = power / (distanceSq+1);
	return attenuation;
}

float RadiusAttenuation(float distanceSq, float radius)
{
	float D = distanceSq; D *= D; D *= D;
	float R = radius; R *= R; R *= R; R *= R;
	return 1.f - saturate(3.f * D / R);
}

[numthreads(10, 10, 8)]
	void InjectPointLightSources(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	float3 samplePoint = CalculateSamplePoint(dispatchThreadId);
	float3 sampleOffset = samplePoint - WorldSpaceView;
	float sampleDistance = length(sampleOffset);
	float3 directionToCentrePoint = sampleOffset / sampleDistance;

	float4 accumulatedLight = 0.f;
	const float beta = 0.1f;
	for (int lightIndex=0; lightIndex<LightCount; lightIndex++) {
		Light l = GetInputLight(lightIndex);
		ProjectedLight p = GetProjectedLight(lightIndex);

		/*
			float Dsv		 = length(p.ViewSpacePosition);
			float Tsv		 = beta * Dsv;
			float cosGamma	 = dot(p.ViewSpacePosition, directionToCentrePoint) / Dsv;
			float sinGamma	 = sqrt(1.f - cosGamma * cosGamma);
			float halfGamma	 = acos(cosGamma)/2.f;
			float Dvp		 = sampleDistance;
			float Tvp		 = beta * sampleDistance;
			float I0		 = l.Power;

			float A0 = beta * I0 * exp(-Tsv * cosGamma) / (2.f * pi * Dsv * sinGamma);
			float A1 = Tsv * sinGamma;
			float v	 = pi * .25f + .5f * atan((Tvp - Tsv * cosGamma) / (Tsv * sinGamma));

			const float uMax = 10.f;
			const float vMax = pi * 0.5f;
			float f1 = AirLightLookup.SampleLevel(DefaultSampler, saturate(float2(A1 / uMax, v / vMax)), 0);
			float f2 = AirLightLookup.SampleLevel(DefaultSampler, saturate(float2(A1 / uMax, halfGamma / vMax)), 0);
			float airLight = A0 * (f1 - f2);
			accumulatedLight += airLight;
		*/

		float3 lightOffset = l.WorldSpacePosition - samplePoint;
		float distanceSq = dot(lightOffset, lightOffset);
		float attenuation = DistanceAttenuation(distanceSq, l.Power);
		attenuation *= RadiusAttenuation(distanceSq, l.Radius);

			//	Basic raleigh scattering equation... Just monochrome scattering for
			//	simplicity
		float cosTheta = dot(-directionToCentrePoint, -lightOffset) * rsqrt(distanceSq);
		float raleighScattering = MonochromeRaleighScattering(cosTheta);
		accumulatedLight.rgb += (0.02f * raleighScattering * attenuation) * l.Colour.rgb;
	}

	InscatterOutput[dispatchThreadId] = accumulatedLight;
}
