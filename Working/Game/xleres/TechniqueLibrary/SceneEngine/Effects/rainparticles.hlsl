// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../Framework/SystemUniforms.hlsl"
#include "../../Framework/CommonResources.hlsl"
#include "../../Math/perlinnoise.hlsl"
#include "../../Math/ProjectionMath.hlsl"
#include "../../Utility/Colour.hlsl"

struct VStoGS
{
	float3 positions[2] : POSITION;
	float radius		: RADIUS;
	float brightness	: BRIGHTNESS;
};

struct GStoPS
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	float brightness : BRIGHTNESS;
};

cbuffer RainSpawn
{
	float3	SpawnMidPoint;
	float3	FallingVelocity;
	int		ParticleCountWidth;
	float	SpawnWidth;
	float	VerticalWrap;
}

	// rain fall velocity should generally be around 8-12 m.sec-1
	//  see:	http://journals.ametsoc.org/doi/pdf/10.1175/1520-0450%281969%29008%3C0249%3ATVORA%3E2.0.CO%3B2
	//			http://www.wired.com/2011/08/how-fast-is-falling-rain/

// static const float3 FallingVelocity = float3(8.1f, 0.1f, -12.f);

Texture2D<float> RandomValuesTexture;

float RandomValue(float2 coord)
{
	uint2 dims;
	RandomValuesTexture.GetDimensions(dims.x, dims.y);
	return RandomValuesTexture.SampleLevel(DefaultSampler, coord * dims, 0);
	// return RandomValuesTexture.Load(uint3(coord.xy % dims.xy, 0));
}

VStoGS vs_main(uint particleId : SV_VertexID)
{
	VStoGS result;

		//	We want to spawn particles randomly at the top of a
		//	volume, and let them fall towards the bottom.
		//	These particles won't interact with the scene, so we don't
		//	need to retain any state about them from frame to frame. Let's
		//	just build their current position procedurally.
		//
		//	All we need is a pattern of start positions along the top of the
		//	volume. Then we can just loop that particle falling through the
		//	same line over and over again.

	int2 id = int2(
		particleId % ParticleCountWidth,
		particleId / ParticleCountWidth);

	float spaceBetweenParticles = SpawnWidth / float(ParticleCountWidth);
	float2 quantStartMidPoint = spaceBetweenParticles * floor(SpawnMidPoint.xy / spaceBetweenParticles);
	float2 startPosition =
		quantStartMidPoint.xy
		+ float2(	spaceBetweenParticles * id.x,
					spaceBetweenParticles * id.y);

		// random variation in speed helps fill in the space with drops
	float fallingSpeedScale			= .5f + .5f * RandomValue(startPosition / 1.7f);
	float3 fallingSpeed				= fallingSpeedScale * FallingVelocity;

	const float verticalWrapTime	= VerticalWrap / -fallingSpeed.z;
	float fallingOffset				= VerticalWrap * RandomValue(startPosition / 2.3f);

	float quantStartHeight			= VerticalWrap * ceil(SpawnMidPoint.z/VerticalWrap);
	float wrappedTime				= verticalWrapTime * frac(SysUniform_GetGlobalTime() / verticalWrapTime);

	float brightnessRandom			= RandomValue(startPosition / 2.487f);
	float length					= (.7f + .6f * brightnessRandom) * 8.0f/60.f;	// streak length (in seconds)

		// need to shift over in xy to guarantee wrapping when there is XY velocity
	float2 xyShiftForWrap = -FallingVelocity.xy * verticalWrapTime * quantStartHeight / VerticalWrap;

	result.positions[1]				= float3(startPosition + xyShiftForWrap, quantStartHeight - fallingOffset) + fallingSpeed * wrappedTime;

		//	we have to wrap the position in XY -- so particles that fall out of our cube will
		//	wrap around to the other side. (Hopefully it should match up with particles in the
		//	next cube over.)
	result.positions[1].xy			= quantStartMidPoint.xy + frac((result.positions[1].xy - quantStartMidPoint) / SpawnWidth) * SpawnWidth;

	result.positions[0]				= result.positions[1] - length * fallingSpeed;
	result.radius					= 0.0012f;
	result.brightness				= .5f + .5f * brightnessRandom;

	return result;
}

[maxvertexcount(4)]
	void gs_main(point VStoGS input[1], inout TriangleStream<GStoPS> outputStream)
{
		//	simple projection... Causes problems when looking straight down or
		//	straight up.
	float4 projected0 = mul(SysUniform_GetWorldToClip(), float4(input[0].positions[0],1));
	float4 projected1 = mul(SysUniform_GetWorldToClip(), float4(input[0].positions[1],1));
	if (InsideFrustum(projected0) || InsideFrustum(projected1)) {
		float width = input[0].radius * projected0.w;

		GStoPS output;
		output.brightness = input[0].brightness;

		output.position = float4(projected0.x + width, projected0.yzw);
		output.texCoord = float2(1.f, 0.f);
		outputStream.Append(output);

		output.position = float4(projected0.x - width, projected0.yzw);
		output.texCoord = float2(0.f, 0.f);
		outputStream.Append(output);

		output.position = float4(projected1.x + width, projected1.yzw);
		output.texCoord = float2(1.f, 1.f);
		outputStream.Append(output);

		output.position = float4(projected1.x - width, projected1.yzw);
		output.texCoord = float2(0.f, 1.f);
		outputStream.Append(output);
	}
}

[earlydepthstencil]
float4 ps_main(GStoPS input) : SV_Target0
{
	// return 1.0.xxxx;

	float brightness =
			(1.f - input.texCoord.x) * input.texCoord.x
		*	(1.f - input.texCoord.y) * input.texCoord.y;
		// brightness = 1.0f - brightness;
	// brightness *= brightness; brightness *= brightness;
	// brightness = 1.0f - brightness;
	return float4(
		LightingScale * 8.f * float3(.33f, .35f, .37f),
		(.5f + 0.5f * brightness) * .25f * .33f * input.brightness);
}
