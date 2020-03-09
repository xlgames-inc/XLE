// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(MAIN_GEOMETRY_H)
#define MAIN_GEOMETRY_H

#define SHADOW_CASCADE_MODE_ARBITRARY 1
#define SHADOW_CASCADE_MODE_ORTHOGONAL 2

#if !defined(VSINPUT_EXTRA)
	#define VSINPUT_EXTRA
#endif

#if !defined(VSOUTPUT_EXTRA)
	#define VSOUTPUT_EXTRA
#endif

#if !defined(VSSHADOWOUTPUT_EXTRA)
	#define VSSHADOWOUTPUT_EXTRA
#endif

struct VSIN //////////////////////////////////////////////////////
{
	#if !defined(GEO_NO_POSITION)
		float3 position : POSITION0;
	#endif

	#if GEO_HAS_COLOR>=1
		float4 color : COLOR0;
	#endif

	#if GEO_HAS_TEXCOORD>=1
		float2 texCoord : TEXCOORD;
	#endif

	#if GEO_HAS_TEXTANGENT==1
		float4 tangent : TEXTANGENT;
	#endif

	#if GEO_HAS_TEXBITANGENT==1
		float3 bitangent : TEXBITANGENT;
	#endif

	#if GEO_HAS_NORMAL==1
		float3 normal : NORMAL;
	#endif

	#if GEO_HAS_BONEWEIGHTS==1
		uint4 boneIndices : BONEINDICES;
		float4 boneWeights : BONEWEIGHTS;
	#endif

	#if GEO_HAS_PARTICLE_INPUTS
		float4 texCoordScale : TEXCOORDSCALE;
		float4 screenRot : PARTICLEROTATION;
		float4 blendTexCoord : TEXCOORD1;
		#define VSOUT_HAS_BLEND_TEXCOORD 1
	#endif

	#if GEO_HAS_VERTEX_ID==1
		uint vertexId : SV_VertexID;
	#endif
	
	#if GEO_HAS_INSTANCE_ID==1
		uint instanceId : SV_InstanceID;
	#endif

	#if GEO_HAS_PER_VERTEX_AO
		float ambientOcclusion : PER_VERTEX_AO;
	#endif

	VSINPUT_EXTRA
}; //////////////////////////////////////////////////////////////////

#if (SPAWNED_INSTANCE==1)
	#define GEO_HAS_INSTANCE_ID 1
	#if !defined(VSOUT_HAS_SHADOW_PROJECTION_COUNT)        // DavidJ -- HACK -- disabling this for shadow shaders
		#define PER_INSTANCE_MLO 1
	#endif
	#if (PER_INSTANCE_MLO==1)
		#define VSOUT_HAS_PER_VERTEX_MLO 1
	#endif
#endif

#if GEO_HAS_COLOR>=1
		// vertex is used only in the vertex shader when
		// "MAT_VCOLOR_IS_ANIM_PARAM" is set. So, in this case,
		// don't output to further pipeline stages.
	#if MAT_VCOLOR_IS_ANIM_PARAM!=1 || VIS_ANIM_PARAM!=0
		#if !defined(VSOUT_HAS_COLOR)
			#if MAT_MODULATE_VERTEX_ALPHA
				#define VSOUT_HAS_COLOR 1
			#else
				#define VSOUT_HAS_COLOR 2
			#endif
		#endif
	#endif
#endif

#if GEO_HAS_TEXCOORD>=1
	#if !defined(VSOUT_HAS_TEXCOORD)
		#define VSOUT_HAS_TEXCOORD 1
	#endif
#endif

#if GEO_HAS_TEXTANGENT==1
	#if RES_HAS_NormalsTexture==1
		#if defined(TANGENT_PROCESS_IN_PS) && TANGENT_PROCESS_IN_PS==1
			#if !defined(VSOUT_HAS_LOCAL_TANGENT_FRAME)
				#define VSOUT_HAS_LOCAL_TANGENT_FRAME 1
			#endif
		#else
			#if !defined(VSOUT_HAS_TANGENT_FRAME)
				#define VSOUT_HAS_TANGENT_FRAME 1
			#endif
		#endif
	#endif
#endif

#if GEO_HAS_NORMAL==1
	#if !defined(VSOUT_HAS_NORMAL)
		#define VSOUT_HAS_NORMAL 1
	#endif
#endif

#if GEO_HAS_PARTICLE_INPUTS
	#define VSOUT_HAS_BLEND_TEXCOORD 1
#endif

#if GEO_HAS_PER_VERTEX_AO
	#if !defined(VSOUT_HAS_PER_VERTEX_AO)
		#define VSOUT_HAS_PER_VERTEX_AO 1
	#endif
#endif

#if (MAT_DO_PARTICLE_LIGHTING==1) && (GEO_HAS_TEXCOORD>=1) && (RES_HAS_NormalsTexture==1)
	#undef VSOUT_HAS_TANGENT_FRAME
	#define VSOUT_HAS_TANGENT_FRAME 1

	#if (RES_HAS_CUSTOM_MAP==1)
		#undef VSOUT_HAS_WORLD_VIEW_VECTOR
		#define VSOUT_HAS_WORLD_VIEW_VECTOR 1
	#endif
#endif

#if GEO_HAS_COLOR>=1 ///////////////////////////////////////////////
	float4 VSIN_GetColor0(VSIN input) { return input.color; }
#else
	float4 VSIN_GetColor0(VSIN input) { return 1.0.xxxx; }
#endif //////////////////////////////////////////////////////////////

#if GEO_HAS_TEXCOORD>=1 /////////////////////////////////////////////
	float2 VSIN_GetTexCoord0(VSIN input) { return input.texCoord; }
#else
	float2 VSIN_GetTexCoord0(VSIN input) { return 0.0.xx; }
#endif //////////////////////////////////////////////////////////////

#if (GEO_HAS_NORMAL==1 || GEO_HAS_TEXTANGENT==1) && (AUTO_COTANGENT==1)
	#undef VSOUT_HAS_TANGENT_FRAME
	#undef VSOUT_HAS_LOCAL_TANGENT_FRAME

		// Can do this in either local or world space -- set VSOUT_HAS_LOCAL_NORMAL & VSOUT_HAS_LOCAL_VIEW_VECTOR for normal space
	#define VSOUT_HAS_NORMAL 1
	#define VSOUT_HAS_WORLD_VIEW_VECTOR 1
#endif

#if MAT_REFLECTIVENESS
	#define VSOUT_HAS_WORLD_VIEW_VECTOR 1       // (need world view vector for the fresnel calculation)
#endif

#if MAT_BLEND_FOG
	#define VSOUT_HAS_FOG_COLOR 1
#endif

struct VSOUT /////////////////////////////////////////////////////
{
	float4 position : SV_Position;
	#if VSOUT_HAS_COLOR>=2
		float3 color : COLOR0;
	#elif VSOUT_HAS_COLOR>=1
		float4 color : COLOR0;
	#endif

	#if VSOUT_HAS_TEXCOORD>=1
		float2 texCoord : TEXCOORD0;
	#endif

	#if VSOUT_HAS_TANGENT_FRAME==1
		float3 tangent : TEXTANGENT;
		float3 bitangent : TEXBITANGENT;
	#endif

	#if VSOUT_HAS_LOCAL_TANGENT_FRAME==1
		float4 localTangent : LOCALTANGENT;
		float3 localBitangent : LOCALBITANGENT;
	#endif

	#if (VSOUT_HAS_NORMAL==1)
		float3 normal : NORMAL;
	#endif

	#if (VSOUT_HAS_LOCAL_NORMAL==1)
		float3 localNormal : LOCALNORMAL;
	#endif

	#if (VSOUT_HAS_LOCAL_VIEW_VECTOR==1)
		float3 localViewVector : LOCALVIEWVECTOR;
	#endif

	#if (VSOUT_HAS_WORLD_VIEW_VECTOR==1)
		float3 worldViewVector : WORLDVIEWVECTOR;
	#endif

	#if (VSOUT_HAS_PRIMITIVE_ID==1)
		nointerpolation uint primitiveId : SV_PrimitiveID;
	#endif

	#if (VSOUT_HAS_RENDER_TARGET_INDEX==1)
		nointerpolation uint renderTargetIndex : SV_RenderTargetArrayIndex;
	#endif

	#if (VSOUT_HAS_WORLD_POSITION==1)
		float3 worldPosition : WORLDPOSITION;
	#endif

	#if (VSOUT_HAS_BLEND_TEXCOORD==1)
		float3 blendTexCoord : TEXCOORD1;
	#endif

	#if (VSOUT_HAS_FOG_COLOR==1)
		float4 fogColor : FOGCOLOR;
	#endif

	#if (VSOUT_HAS_PER_VERTEX_AO==1)
		float ambientOcclusion : AMBIENTOCCLUSION;
	#endif

	#if (VSOUT_HAS_PER_VERTEX_MLO==1)
		float mainLightOcclusion : MAINLIGHTOCCLUSION;
	#endif

	#if (VSOUT_HAS_INSTANCE_ID==1)
		uint instanceId : SV_InstanceID;
	#endif

	VSOUTPUT_EXTRA
}; //////////////////////////////////////////////////////////////////

#endif
