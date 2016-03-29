// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(OCEAN_H)
#define OCEAN_H

cbuffer OceanMaterialSettings
{
        // these settings are also used while simulating the
        // shallow water
    float PhysicalWidth, PhysicalHeight;
    float StrengthConstantXY;
    float StrengthConstantZ;
    float ShallowGridPhysicalDimension;
    float WaterBaseHeight;

	float FoamThreshold = .3f;
	float FoamIncreaseSpeed = 8.f / .33f;
	float FoamIncreaseClamp = 8.f;
	int FoamDecrease = 1;
}

// #define DO_FREQ_BOOST 1
#if DO_FREQ_BOOST==1
    static const float StrengthConstantMultiplier = 4.f * 1.0f / 512.f; //256.f / 2.f;
#else
    static const float StrengthConstantMultiplier = 1.0f / 512.f; // 256.f / 2.f;
#endif

uint2 OceanClp(int2 coords, uint2 texDimensions)
{
    // note -- expecting power of 2 for "texDimensions" here!
    // modulo operation doesn't handle negative values correctly, but a bitwise operation will.
    return uint2(coords.x&(texDimensions.x-1), coords.y&(texDimensions.y-1));
}

float OceanTextureCustomInterpolate(Texture2D<float> inputTex, uint2 texDimensions, float2 textureCoords)
{
		//	we need a custom interpolation, because each grid cell changes
		//	sign in a chess-board pattern
    float2 explodedCoords = textureCoords * float2(texDimensions);
	int2 baseCoords = int2(explodedCoords);
	float sample00 = inputTex[OceanClp(baseCoords + int2(0,0), texDimensions)];
	float sample10 = inputTex[OceanClp(baseCoords + int2(1,0), texDimensions)];
	float sample01 = inputTex[OceanClp(baseCoords + int2(0,1), texDimensions)];
	float sample11 = inputTex[OceanClp(baseCoords + int2(1,1), texDimensions)];

	if (((baseCoords.x + baseCoords.y)&1)==1) {
		sample00 = -sample00;
		sample11 = -sample11;
	} else {
		sample01 = -sample01;
		sample10 = -sample10;
	}

	float2 fractionalPart = frac(explodedCoords);
    float weight00 = (1.f - fractionalPart.x) * (1.f - fractionalPart.y);
    float weight01 = (1.f - fractionalPart.x) * fractionalPart.y;
    float weight10 = fractionalPart.x * (1.f - fractionalPart.y);
    float weight11 = fractionalPart.x * fractionalPart.y;
	return	sample00 * weight00
		+	sample01 * weight01
		+	sample10 * weight10
		+	sample11 * weight11
		;
}

#endif
