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

///////////////////////////////////////////////////////////////////////////////////////////////////
	//		F O R W A R D   M A I N
///////////////////////////////////////////////////////////////////////////////////////////////////

~forward_main; item=<:(

#include "TextureAlgorithm.h" 	// for SystemInputs
#include "Lighting/Forward.h"

#if !((OUTPUT_TEXCOORD==1) && (MAT_ALPHA_TEST==1))
	[earlydepthstencil]
#endif
float4 forward_main({{MainFunctionParameterSignature}}, SystemInputs sys) : SV_Target0
{
    // If we're doing to do the alpha threshold test, we
    // should try to do as early in the shader as we can!
    // Unfortunately, there's no real easy way to do that with
    // a node graph here...Unless we create some special #define
    // somehow...

	// DoAlphaTest(geo, GetAlphaThreshold());

	GBufferValues sample;
    {{GraphName}}({{ForwardMainParameters}}, sample);

	float3 directionToEye = 0.0.xxx;
	#if (OUTPUT_WORLD_VIEW_VECTOR==1)
		directionToEye = normalize(geo.worldViewVector);
	#endif

	float4 result = float4(
		ResolveLitColor(
			sample, directionToEye, GetWorldPosition(geo),
			LightScreenDest_Create(int2(geo.position.xy), GetSampleIndex(sys))), 1.f);

	#if OUTPUT_FOG_COLOR == 1
		result.rgb = geo.fogColor.rgb + result.rgb * geo.fogColor.a;
	#endif

	result.a = sample.blendingAlpha;

    #if (OUTPUT_COLOUR>=1) && (MAT_VCOLOR_IS_ANIM_PARAM==0)
        result.rgb *= geo.colour.rgb;
    #endif

	#if MAT_SKIP_LIGHTING_SCALE==0
		result.rgb *= LightingScale;		// (note -- should we scale by this here? when using this shader with a basic lighting pipeline [eg, for material preview], the scale is unwanted)
	#endif
	return result;
}

):>

///////////////////////////////////////////////////////////////////////////////////////////////////
	//		O R D E R   I N D E P E N D E N T   M A I N
///////////////////////////////////////////////////////////////////////////////////////////////////

~oi_main; item=<:(

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
	const float maxAlpha = 0.95f; // 254.f / 255.f;  // AlphaThreshold;
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

///////////////////////////////////////////////////////////////////////////////////////////////////
	//		S T O C H A S T I C   M A I N
///////////////////////////////////////////////////////////////////////////////////////////////////

~stochastic_main;  item=<:(

[earlydepthstencil]
float4 stochastic_main(VSOutput geo,
	#if (STOCHASTIC_TRANS_PRIMITIVEID==1)
		uint primitiveID : SV_PrimitiveID,
	#endif
	SystemInputs sys) : SV_Target
{
	float occlusion;
	uint type = CalculateStochasticPixelType(geo.position, occlusion);
	[branch] if (type > 0) {
		if (type == 2) return float4(0.0.xxx, 1); // discard;

		// Only need to calculate the "alpha" value for this step...
		GBufferValues sample;
		{{GraphName}}({{ForwardMainParameters}}, sample);
		return float4(0.0.xxx, sample.blendingAlpha);
	}

	GBufferValues sample;
	{{GraphName}}({{ForwardMainParameters}}, sample);

	float4 litValue = LightSample(sample, geo, sys);
	return float4((LightingScale * (1.f - occlusion) * litValue.a) * litValue.rgb, litValue.a);
}

):>

///////////////////////////////////////////////////////////////////////////////////////////////////
	//		D E P T H   O N L Y
///////////////////////////////////////////////////////////////////////////////////////////////////

~depthonly_main;  item=<:(

#include "Forward/Transparency/depthonlyutil.h"

#if (STOCHASTIC_TRANS)

	void depthonly_main(
		VSOutput geo, uint primitiveID : SV_PrimitiveID,
		out uint oCoverage : SV_Coverage
		#if (STOCHASTIC_TRANS_PRIMITIVEID==1)
			, out uint oPrimId : SV_Target0
			#if (STOCHASTIC_TRANS_OPACITY==1)
				, out float oOpacity : SV_Target1
			#endif
		#elif (STOCHASTIC_TRANS_OPACITY==1)
			, out float oOpacity : SV_Target0
		#endif

		)
	{
		GBufferValues sample;
		{{GraphName}}({{ForwardMainParameters}}, sample);
		float alpha = sample.blendingAlpha;

		oCoverage = StochasticTransMask(uint2(geo.position.xy), alpha, primitiveID);
		#if (STOCHASTIC_TRANS_PRIMITIVEID==1)
			oPrimId = primitiveID;
		#endif
		#if (STOCHASTIC_TRANS_OPACITY==1)
			oOpacity = alpha;
		#endif
	}

#else

	#if !((OUTPUT_TEXCOORD==1) && ((MAT_ALPHA_TEST==1)||(MAT_ALPHA_TEST_PREDEPTH==1)))
		[earlydepthstencil]
	#endif
	void depthonly_main(VSOutput geo)
	{
		#if !((OUTPUT_TEXCOORD==1) && ((MAT_ALPHA_TEST==1)||(MAT_ALPHA_TEST_PREDEPTH==1)))
				// execute sampling only for discard() events
			GBufferValues sample;
			{{GraphName}}({{ForwardMainParameters}}, sample);
		#endif
	}

#endif

):>
