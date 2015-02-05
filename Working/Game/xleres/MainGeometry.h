// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(MAIN_GEOMETRY_H)
#define MAIN_GEOMETRY_H

#if !defined(VSOUTPUT_EXTRA)
    #define VSOUTPUT_EXTRA
#endif

#if (SPAWNED_INSTANCE==1)
    #define GEO_HAS_INSTANCE_ID 1
    #if (PER_INSTANCE_AO==1)
        #define OUTPUT_PER_VERTEX_AO 1
    #endif
#endif

struct VSInput //////////////////////////////////////////////////////
{
	float3 position : POSITION0;
	
	#if GEO_HAS_COLOUR==1
		float4 colour : COLOR0;
        #if !defined(OUTPUT_COLOUR)
		    #define OUTPUT_COLOUR 1
        #endif
	#endif

	#if GEO_HAS_TEXCOORD==1
		float2 texCoord : TEXCOORD;
		#undef OUTPUT_TEXCOORD
		#define OUTPUT_TEXCOORD 1
	#endif
	
	#if GEO_HAS_TANGENT_FRAME==1
		float4 tangent : TANGENT;
		float3 bitangent : BITANGENT;
        #if TANGENT_PROCESS_IN_PS==1
            #undef OUTPUT_LOCAL_TANGENT_FRAME
		    #define OUTPUT_LOCAL_TANGENT_FRAME 1
        #else
		    #undef OUTPUT_TANGENT_FRAME
		    #define OUTPUT_TANGENT_FRAME 1
        #endif
	#endif
	
	#if GEO_HAS_NORMAL==1
		float3 normal : NORMAL;
        #undef OUTPUT_NORMAL
        #define OUTPUT_NORMAL 1
	#endif

	#if GEO_HAS_SKIN_WEIGHTS==1
		uint4 boneIndices : BONEINDICES;
		float4 boneWeights : BONEWEIGHTS;
	#endif

	#if GEO_HAS_PARTICLE_INPUTS
		float4 texCoordScale : TEXCOORDSCALE;
		float4 screenRot : PARTICLEROTATION;
        float4 blendTexCoord : TEXCOORD1;
        #define OUTPUT_BLEND_TEXCOORD 1
	#endif

    #if GEO_HAS_INSTANCE_ID==1
        // float4 instanceOffset : INSTANCE_OFFSET;
        uint instanceId : SV_InstanceID;
    #endif
}; //////////////////////////////////////////////////////////////////

#if (MAT_DO_PARTICLE_LIGHTING==1) && (GEO_HAS_TEXCOORD==1) && (RES_HAS_NORMAL_MAP==1)
    #undef OUTPUT_TANGENT_FRAME
    #define OUTPUT_TANGENT_FRAME 1

    #if (RES_HAS_CUSTOM_MAP==1)
        #undef OUTPUT_WORLD_VIEW_VECTOR
        #define OUTPUT_WORLD_VIEW_VECTOR 1
    #endif
#endif

#if GEO_HAS_COLOUR==1 ///////////////////////////////////////////////
	float4 GetColour(VSInput input) { return input.colour; }
#else
	float4 GetColour(VSInput input) { return 1.0.xxxx; }
#endif //////////////////////////////////////////////////////////////

#if GEO_HAS_TEXCOORD==1 /////////////////////////////////////////////
	float2 GetTexCoord(VSInput input) { return input.texCoord; }
#else
	float2 GetTexCoord(VSInput input) { return 1.0.xx; }
#endif //////////////////////////////////////////////////////////////

#if (GEO_HAS_NORMAL==1 || GEO_HAS_TANGENT_FRAME==1) && (AUTO_COTANGENT==1)
	#undef OUTPUT_TANGENT_FRAME

        // Can do this in either local or world space -- set OUTPUT_LOCAL_NORMAL & OUTPUT_LOCAL_VIEW_VECTOR for normal space
	#define OUTPUT_NORMAL 1
	#define OUTPUT_WORLD_VIEW_VECTOR 1
#endif

#if MAT_REFLECTIVENESS
    #define OUTPUT_WORLD_VIEW_VECTOR 1       // (need world view vector for the fresnel calculation)
#endif

#if MAT_DOUBLE_SIDED
    #define OUTPUT_WORLD_VIEW_VECTOR 1
#endif

#if MAT_BLEND_FOG
    #define OUTPUT_FOG_COLOR 1
#endif

struct VSOutput /////////////////////////////////////////////////////
{
	float4 position : SV_Position;
	#if OUTPUT_COLOUR>=2
		float3 colour : COLOR0;
    #elif OUTPUT_COLOUR>=1
        float4 colour : COLOR0;
	#endif

	#if OUTPUT_TEXCOORD==1
		float2 texCoord : TEXCOORD0;
	#endif
	
	#if OUTPUT_TANGENT_FRAME==1
		float3 tangent : TANGENT;
		float3 bitangent : BITANGENT;
	#endif

    #if OUTPUT_LOCAL_TANGENT_FRAME==1
		float4 localTangent : LOCALTANGENT;
		float3 localBitangent : LOCALBITANGENT;
	#endif
	
	#if (OUTPUT_TANGENT_FRAME==1) || (OUTPUT_NORMAL==1)
		float3 normal : NORMAL;
	#endif
	
    #if (OUTPUT_LOCAL_NORMAL==1)
		float3 localNormal : LOCALNORMAL;
	#endif

	#if (OUTPUT_LOCAL_VIEW_VECTOR==1)
		float3 localViewVector : LOCALVIEWVECTOR;
	#endif

    #if (OUTPUT_WORLD_VIEW_VECTOR==1)
		float3 worldViewVector : WORLDVIEWVECTOR;
	#endif
	
	#if (OUTPUT_RENDER_TARGET_INDEX==1)
		uint renderTargetIndex : SV_RenderTargetArrayIndex;
	#endif

    #if (OUTPUT_WORLD_POSITION==1)
        float3 worldPosition : WORLDPOSITION;
    #endif

    #if (OUTPUT_BLEND_TEXCOORD==1)
        float3 blendTexCoord : TEXCOORD1;
    #endif

    #if (OUTPUT_FOG_COLOR==1)
        float4 fogColor : FOGCOLOR;
    #endif

    #if (OUTPUT_PER_VERTEX_AO==1)
        float ambientOcclusion : AMBIENTOCCLUSION;
    #endif

    VSOUTPUT_EXTRA
}; //////////////////////////////////////////////////////////////////

struct VSShadowOutput /////////////////////////////////////////////////////
{
    float3 position : POSITION0;

	#if OUTPUT_TEXCOORD==1
		float2 texCoord : TEXCOORD0;
	#endif

	#if SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ARBITRARY
        #if (OUTPUT_SHADOW_PROJECTION_COUNT>0)
            float4 shadowPosition[OUTPUT_SHADOW_PROJECTION_COUNT] : SHADOWPOSITION;
        #endif
    #elif SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ORTHOGONAL
        float3 baseShadowPosition : SHADOWPOSITION;
    #endif

    #if (OUTPUT_SHADOW_PROJECTION_COUNT>0)
        uint shadowFrustumFlags : SHADOWFLAGS;
    #endif

    VSOUTPUT_EXTRA
}; //////////////////////////////////////////////////////////////////


#endif
