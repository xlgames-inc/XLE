~deferred_main; item=<:(

#if !((OUTPUT_TEXCOORD==1) && (MAT_ALPHA_TEST==1))
	[earlydepthstencil]
#endif
GBufferEncoded deferred_main({{MainFunctionParameterSignature}})
{
    // If we're doing to do the alpha threshold test, we
    // should try to do as early in the shader as we can!
    // Unfortunately, there's no real easy way to do that with
    // a node graph here...Unless we create some special #define
    // somehow...
	// DoAlphaTest(geo, GetAlphaThreshold());
	GBufferValues result;
    {{GraphName}}({{ForwardMainParameters}}, result);
	return Encode(result);
}

):>

~oi_main;  item=<:(

#include "game/xleres/Forward/Transparency/util.h"

float4 io_main({{MainFunctionParameterSignature}}, SystemInputs sys) : SV_Target0
{
    float destinationDepth = DuplicateOfDepthBuffer[uint2(geo.position.xy)];
	float ndcComparison = geo.position.z; // / geo.position.w;
	if (ndcComparison > destinationDepth)
		discard;

    GBufferValues sample;
    {{GraphName}}({{ForwardMainParameters}}, sample);

		// note --  At alpha threshold, we just consider
		//			it opaque. It's a useful optimisation
		//			that goes hand in hand with the pre-depth pass.
	const float minAlpha =   1.f / 255.f;
	const float maxAlpha = 254.f / 255.f;  // AlphaThreshold;
	if (sample.blendingAlpha < minAlpha) {
		discard;
	}

    float4 result = LightSample(sample, geo, sys);

    if (result.a >= maxAlpha) {
		return float4(LightingScale * result.rgb, 1.f); // result.a);
	} else {
		#if !MAT_PREMULTIPLIED_ALPHA
			result.rgb *= result.a;
		#endif

		OutputFragmentNode(uint2(geo.position.xy), result, ndcComparison);
		discard;
		return 0.0.xxxx;
	}
}

):>
