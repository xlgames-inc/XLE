// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GeometryConfiguration.hlsl"
#include "../TechniqueLibrary/Framework/MainGeometry.hlsl"
#include "../TechniqueLibrary/Framework/Surface.hlsl"
#include "../TechniqueLibrary/Math/Misc.hlsl"
#include "../BasicMaterial.hlsl"

void DoDitherAlphaTest(VSOUT geo, int2 pixelCoords, uint primitiveID)
{
		// This kind of dithering is going to sometimes produce some
		// wierd artefacts (and anyway, it will shake around a lot).
		// It's only really viable if there is a lot of blurring happening
		// in the shadow resolve stage.
		// It can create streaks up close, as well as blurring in the distance.
		//
		// We can also write a translucency value to a second bound render
		// target (perhaps with a "max" blend). This could be used to soften
		// the shadows cast by translucent geometry.
	#if (VSOUT_HAS_TEXCOORD>=1)
		float alphaValue = DiffuseTexture.Sample(DefaultSampler, geo.texCoord.xy).a;

			// integrating in the primitive id here gives us a better result when many
			// layers are on top of each other (otherwise each layer would all get the same mask)
		pixelCoords.x += primitiveID;
		pixelCoords.y += primitiveID>>2;
		float ditherBias = DitherPatternValue(pixelCoords);
		//float ditherBias =
		//	IntegerHash(primitiveID + pixelCoords.x<<10 + pixelCoords.y<<20) / float(0xffffffff);
		clip(alphaValue - .75f * ditherBias - 1e-6f);
	#endif
}

#if !((VSOUT_HAS_TEXCOORD>=1) && ((MAT_ALPHA_TEST==1)||(MAT_ALPHA_DITHER_SHADOWS==1))) && (VULKAN!=1)
	[earlydepthstencil]
#endif
void main(VSOUT geo)
{
	#if (VSOUT_HAS_TEXCOORD>=1) && (MAT_ALPHA_DITHER_SHADOWS==1)
		// DoDitherAlphaTest(geo, int2(geo.position.xy), geo.primitiveId);
		AlphaTestAlgorithm(DiffuseTexture, DefaultSampler, geo.texCoord, 0.5f);
	#else
		DoAlphaTest(geo, AlphaThreshold);
	#endif
}
