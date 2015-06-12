// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(TERRAIN_LAYER_H)
#define TERRAIN_LAYER_H

struct TerrainLayerVSOutput /////////////////////////////////////////
{
	float4 position 	: SV_Position;
	float4 colour 		: COLOR0;
	float2 texCoord 	: TEXCOORD0;
	float2 baseTexCoord : TEXCOORD1;
	float3 normal		: NORMAL;

	#if OUTPUT_LOCAL_VIEW_VECTOR==1
        float3 localViewVector 	: LOCALVIEWVECTOR;
    #endif

    #if OUTPUT_WORLD_VIEW_VECTOR==1
        float3 worldViewVector 	: WORLDVIEWVECTOR;
    #endif
	
	#if OUTPUT_TANGENT_FRAME==1
		float3 tangent : TEXTANGENT;
		float3 bitangent : TEXBITANGENT;
	#endif

    #if (OUTPUT_FOG_COLOR==1)
        float4 fogColor : FOGCOLOR;
    #endif
}; //////////////////////////////////////////////////////////////////

#endif

