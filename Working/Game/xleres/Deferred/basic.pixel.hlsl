// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../TechniqueLibrary/Framework/MainGeometry.hlsl"
#include "../Objects/IllumShader/PerPixel.h"

#if !((VSOUT_HAS_TEXCOORD==1) && (MAT_ALPHA_TEST==1)) && (VULKAN!=1)
	[earlydepthstencil]
#endif
GBufferEncoded main(VSOUT geo)
{
	DoAlphaTest(geo, GetAlphaThreshold());

	#if (VIS_ANIM_PARAM!=0) && (VSOUT_HAS_COLOR==1)
		{
			GBufferValues visResult = GBufferValues_Default();
			#if VIS_ANIM_PARAM==1
				visResult.diffuseAlbedo = geo.color.rrr;
			#elif VIS_ANIM_PARAM==2
				visResult.diffuseAlbedo = geo.color.ggg;
			#elif VIS_ANIM_PARAM==3
				visResult.diffuseAlbedo = geo.color.bbb;
			#elif VIS_ANIM_PARAM==4
				visResult.diffuseAlbedo = geo.color.aaa;
			#elif VIS_ANIM_PARAM==5
				visResult.diffuseAlbedo = geo.color.rgb;
			#endif
			return Encode(visResult);
		}
	#endif

	GBufferValues result = IllumShader_PerPixel(geo);
	return Encode(result);
}

GBufferEncoded invalid(VSOUT geo)
{
	float3 color0 = float3(1.0f, 0.f, 0.f);
	float3 color1 = float3(0.0f, 0.f, 1.f);
	uint flag = (uint(geo.position.x/4.f) + uint(geo.position.y/4.f))&1;
	GBufferValues result = GBufferValues_Default();
	result.diffuseAlbedo = flag?color0:color1;
	return Encode(result);
}
