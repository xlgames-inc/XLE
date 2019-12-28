#pragma once

//
// This is a collection of strings, which get mounted as virtual files when running unit tests
// It's a little more convenient than having actual data files hanging around in the filesystem
// somewhere. This way the data is associated with the code, and will not get in the way when
// running non-unit test executables.
//

static const char* s_examplePerPixelShaderFile = R"--(
	#include "xleres/MainGeometry.h"
	#include "xleres/CommonResources.h"
	#include "xleres/gbuffer.h"

	Texture2D       Texture0		BIND_MAT_T0;		// Diffuse

	cbuffer BasicMaterialConstants
	{
		float4 HairColor;
	}

	GBufferValues PerPixel(VSOutput geo)
	{
		GBufferValues result = GBufferValues_Default();

		float4 diffuseTextureSample = 1.0.xxxx;
		#if (OUTPUT_TEXCOORD==1) && (RES_HAS_Texture0!=0)
			diffuseTextureSample = Texture0.Sample(MaybeAnisotropicSampler, geo.texCoord);
			result.diffuseAlbedo = diffuseTextureSample.rgb;
			result.blendingAlpha = diffuseTextureSample.a;
		#endif

		return result;
	}
)--";


static const char* s_exampleGraphFile = R"--(
	import example_perpixel = "example-perpixel.psh";
	import templates = "xleres/nodes/templates.sh"

	GBufferValues Bind_PerPixel(VSOutput geo) implements templates::PerPixel
	{
		return example_perpixel::PerPixel(geo:geo).result;
	}
)--";

