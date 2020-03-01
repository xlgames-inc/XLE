// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../TechniqueLibrary/Math/Misc.hlsl"		// (for IntegerHash)

#if (STOCHASTIC_TRANS)

	Texture2D<uint> StochasticTransMasks : register(t18);

	// GetLayerSeed & StochasticTransMask implementations are from
	// the directX sample (described in the paper, "Stochastic Transparency")

	uint GetLayerSeed(uint2 pixelPos, int primID)
	{
		// Calculate a random seed based on the pixel position
		// and primitive id.

		// Seeding by primitive id, as described in Section 5 of the paper.
		uint layerseed = primID * 32;

		// For simulating more than 8 samples per pixel, the algorithm
		// can be run in multiple passes with different random offsets.
		// layerseed += g_randomOffset;

		layerseed += (pixelPos.x << 10) + (pixelPos.y << 20);
		return layerseed;
	}

	static const uint RandMaskAlphaValues = 0xff;
	static const uint RandMaskSizePowOf2MinusOne = 2048-1;

	uint StochasticTransMask(uint2 pixelPos, float alpha, int primID)
	{
		// We're going to calculate a coverage mask that suits this alpha
		// value. We want to use a random mask based on the screen position
		// and primitive id.
		// First, we calculate a seed to select a random mask. But we hash
		// this to prevent any screen space patterns showing up.
		uint seed = GetLayerSeed(pixelPos, primID);
		seed = IntegerHash(seed);

		// Clamping texture coordinates (bit fiddling because the texture
		// has power of 2 dimensions)
		seed &= RandMaskSizePowOf2MinusOne;

		return StochasticTransMasks.Load(int3(seed, alpha * RandMaskAlphaValues, 0));
	}

#endif
