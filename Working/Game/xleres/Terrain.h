// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(TERRAIN_H)
#define TERRAIN_H

cbuffer TerrainTile
{
	float4 TerrainBaseMapping;
	row_major float4x4 TerrainLayerMapping;
	row_major float4x4 TerrainLayerMappingDistortion;
	float4 TerrainLayerMapping_Params;
}

#if (RES_HAS_NORMAL_MAP==1)
	#undef OUTPUT_TANGENT_FRAME
	#define OUTPUT_TANGENT_FRAME 1
#endif

#endif
