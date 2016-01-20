// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../gbuffer.h"
#include "../../MainGeometry.h"
#include "../../TextureAlgorithm.h"     // (for SystemInputs)
#include "../../Surface.h"
#include "../Lighting/LightDesc.h"
#include "../Lighting/Forward.h"

struct FragmentListNode
{
	uint	next;
	float	depth;
	uint	color;
};

uint FragmentListNode_PackColor(float4 color)
{
	color = saturate(color);
	return uint(color.a*255.f)<<24
		|  uint(color.r*255.f)<<16
		|  uint(color.g*255.f)<< 8
		|  uint(color.b*255.f)
		;
}

RWTexture2D<uint>						FragmentIds : register(u1);
RWStructuredBuffer<FragmentListNode>	NodesList	: register(u2);

Texture2D<float>						DuplicateOfDepthBuffer : register(t17);

void OutputFragmentNode(uint2 position, float4 color, float depth)
{
		//
		//	todo -- we could output a self balancing binary search tree...
		//			just might reduce the workload while resolving the sort
		//			order (particularly if we have a lot of elements, and need
		//			to do a partial sort)
		//				-- but it would require read/write access to NodesList,
		//					which might not be thread safe.
		//
	uint newNodeId = NodesList.IncrementCounter();
	uint oldNodeId;
	InterlockedExchange(FragmentIds[position], newNodeId, oldNodeId);

	FragmentListNode newNode;
	newNode.next = oldNodeId;
	newNode.depth = depth;
	newNode.color = FragmentListNode_PackColor(color);
	NodesList[newNodeId] = newNode;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
		// Do the full lighting here --
		// 	actually, it might be better to do some of the lighting after resolve
		//	If we do a shadow lookup here (for example), it will mean doing a
		//	separate shadow lookup on each layer. But we might only really need
		//	that for the top-most few layers.
		//	One way to deal with this is to do an early depth pass that would set
		//	the depth buffer depth for fully opaque parts; or just after a fixed
		//	number of layers...?
float4 LightSample(GBufferValues sample, VSOutput geo, SystemInputs sys)
{
	float3 directionToEye = 0.0.xxx;
	#if (OUTPUT_WORLD_VIEW_VECTOR==1)
		directionToEye = normalize(geo.worldViewVector);
	#endif

	float4 result = float4(
		ResolveLitColor(
			sample, directionToEye, GetWorldPosition(geo),
			LightScreenDest_Create(int2(geo.position.xy), GetSampleIndex(sys))), 1.f);

	#if OUTPUT_FOG_COLOR == 1
		result.rgb = lerp(geo.fogColor.rgb, result.rgb, geo.fogColor.a);
	#endif

	result.a = sample.blendingAlpha;
	return result;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
