// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#undef GEO_HAS_COLOUR
#undef GEO_HAS_NORMAL

#include "../Transform.h"
#include "../MainGeometry.h"
#include "../Terrain.h"
#include "../TerrainSurface.h"

VSInput main(VSInput input)
{
	float3 localPosition = GetLocalPosition(input);
	input.position = mul(LocalToWorld, float4(localPosition,1)).xyz;
	return input;
}
