// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)


// This is just a rough experiment that was used to test the
// order independant transparency

static const float SqrtHalf = 0.70710678f;
static const float3 NegativeLightDirection = float3(SqrtHalf, 0.f, SqrtHalf);

// float Square(float x) { return x*x; }

// float SchlickFresnel(float3 viewDirection, float3 halfVector, float refractiveIndex)
// {
// 		// (note -- the 1.f here assumes one side of the interface is air)
// 	float f0 = Square((1.f - refractiveIndex) / (1.f + refractiveIndex));
//
// 	float A = 1.0f - saturate(dot(viewDirection, halfVector));
// 	float sq = A*A;
// 	float cb = sq*sq;
// 	float q = cb*A;
//
// 	return f0 + (1.f - f0) * q;	// (note, use lerp for this..?)
// }

float FresnelForReflection_Local(VSOUT geo, float refractiveIndex)
{
    float3 localSpaceNormal = normalize(geo.localNormal);
    float3 localViewDirection = normalize(geo.localViewVector);
    float3 localSpaceReflection = reflect(-localViewDirection, localSpaceNormal);

    // float3 halfVector = normalize(NegativeLightDirection + localViewDirection);
    float3 halfVector = normalize(localSpaceReflection + localViewDirection);
    return SchlickFresnel(localViewDirection, halfVector, refractiveIndex);
}

float FresnelForReflection_World(VSOUT geo, float refractiveIndex)
{
    float3 worldSpaceNormal = VSOUT_GetNormal(geo);
    float3 worldViewDirection = normalize(geo.worldViewVector);
    float3 worldSpaceReflection = reflect(-worldViewDirection, worldSpaceNormal);

    float3 halfVector = normalize(worldSpaceReflection + worldViewDirection);
    return SchlickFresnel(worldViewDirection, halfVector, refractiveIndex);
}

// Texture2D ReflectionBox12	: register(t8);
// Texture2D ReflectionBox34	: register(t9);
// Texture2D ReflectionBox5	: register(t10);

Texture2D SkyReflectionTexture[3] : register(t7);

void GlassShader(VSOUT geo)
{
    float3 worldSpaceNormal = VSOUT_GetNormal(geo);
    // worldSpaceNormal = normalize(worldSpaceNormal);

    // float noiseValue2 = PerlinNoise3D(45.f * geo.worldPosition);
    // noiseValue2 = .5 + .5 * noiseValue2;
    // OutputFragmentNode(uint2(geo.position.xy), float4(1,1,1,noiseValue2), geo.position.z);
    // return;

    float3 noiseDerivative;
    float noiseValue = PerlinNoise3DDev(
        /*45.f * */ 15.f * geo.worldPosition, noiseDerivative);
    noiseValue = 0.5f + 0.5f * noiseValue;
    worldSpaceNormal += 0.075f * (noiseDerivative);
    worldSpaceNormal = normalize(worldSpaceNormal);

    const float refractiveIndex = 1.9f;
    float d = FresnelForReflection_World(geo, refractiveIndex);

    uint2 reflectionTextureDims;
    // ReflectionBox12.GetDimensions(reflectionTextureDims.x, reflectionTextureDims.y);
    SkyReflectionTexture[0].GetDimensions(reflectionTextureDims.x, reflectionTextureDims.y);

    float3 worldViewDirection = normalize(geo.worldViewVector);
    float3 worldSpaceReflection = reflect(-worldViewDirection, worldSpaceNormal);
    //float4 reflectionLookup = ReadReflectionHemiBox(
    //	worldSpaceReflection,
    //	ReflectionBox12, ReflectionBox34, ReflectionBox5,
    //	reflectionTextureDims, 4);
    float2 skyReflectionCoord = DirectionToEquirectangularCoord_YUp(worldSpaceReflection);
    float4 reflectionLookup = SkyReflectionTexture[0].Load(
        int3(skyReflectionCoord.xy * float2(reflectionTextureDims.xy), 0));

    // float3 localSpaceNormal = normalize(geo.localNormal);
    // float3 localViewDirection = normalize(geo.localViewVector);
    // float3 localSpaceReflection = reflect(-localViewDirection, localSpaceNormal);
    //
    // // float3 halfVector = normalize(NegativeLightDirection + localViewDirection);
    // float3 halfVector = normalize(localSpaceReflection + localViewDirection);
    // const float refractiveIndex = 1.05f;
    // float d = SchlickFresnel(localViewDirection, halfVector, refractiveIndex);

    // d = 1.f-abs(dot(localViewDirection,localSpaceNormal));
    // d = saturate(dot(halfVector, localSpaceNormal));

    // localSpaceReflection = reflect(-localViewDirection, localSpaceNormal);
    // d = saturate(dot(localSpaceReflection, NegativeLightDirection));
    // d = pow(d,32.f);

    reflectionLookup.rgb = 5.f * pow(reflectionLookup.rgb, 4.f);
    reflectionLookup.rgb = lerp(SRGBLuminance(reflectionLookup.rgb).xxx, reflectionLookup.rgb, 0.25f);
    reflectionLookup.rgb += 8.0.xxx * pow(saturate(dot(worldSpaceReflection, NegativeLightDirection)), 128.f);

    float alpha = 0.5f; // lerp(0.15f, 1.f, d);
    const float minAlpha =   4.f / 255.f;
    if (alpha > minAlpha) {
        // const float3 baseColor = 0.75.xxx;
        alpha *= reflectionLookup.a;
        const float3 baseColor = reflectionLookup.rgb;
        float4 finalColor = float4(baseColor*lerp(0.1f, 1.f, d), alpha);

        // finalColor = float4(0.5f + 0.5f * worldSpaceNormal.xyz, 1);
        OutputFragmentNode(uint2(geo.position.xy), finalColor, geo.position.z);
    }
}
