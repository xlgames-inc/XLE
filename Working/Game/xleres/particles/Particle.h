// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Transform.h"
#include "../MainGeometry.h"

struct ParticleVStoGS
{
    float3 position : POSITION0;
	
	#if GEO_HAS_COLOR==1
		float4 colour : COLOR0;
	#endif

	#if GEO_HAS_TEXCOORD==1
		float2 texCoord : TEXCOORD;
	#endif

    float4 texCoordScale : TEXCOORDSCALE;
	float4 screenRot : PARTICLEROTATION;

    #if (OUTPUT_FOG_COLOR==1)
        float4 fogColor : FOGCOLOR;
    #endif
};

