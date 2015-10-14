// Copyright 2015 XLGAMES Inc.
//
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

            // vertex is used only in the vertex shader when
            // "MAT_VCOLOR_IS_ANIM_PARAM" is set. So, in this case,
            // don't output to further pipeline stages.
        #if MAT_VCOLOR_IS_ANIM_PARAM!=1 || VIS_ANIM_PARAM!=0
            #if !defined(OUTPUT_COLOUR)
                #define OUTPUT_COLOUR 1
            #endif
        #endif
    #endif

    #if GEO_HAS_TEXCOORD==1
        float2 texCoord : TEXCOORD;
        #if !defined(OUTPUT_TEXCOORD)
            #define OUTPUT_TEXCOORD 1
        #endif
    #endif

    #if GEO_HAS_TANGENT_FRAME==1
        float4 tangent : TEXTANGENT;
        #if RES_HAS_NormalsTexture==1
            #if TANGENT_PROCESS_IN_PS==1
                #if !defined(OUTPUT_LOCAL_TANGENT_FRAME)
                    #define OUTPUT_LOCAL_TANGENT_FRAME 1
                #endif
            #else
                #if !defined(OUTPUT_TANGENT_FRAME)
                    #define OUTPUT_TANGENT_FRAME 1
                #endif
            #endif
        #endif
    #endif

    #if GEO_HAS_BITANGENT==1
        float3 bitangent : TEXBITANGENT;
    #endif

    #if GEO_HAS_NORMAL==1
        float3 normal : NORMAL;
        #if !defined(OUTPUT_NORMAL)
            #define OUTPUT_NORMAL 1
        #endif
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

    #if GEO_HAS_PER_VERTEX_AO
        float ambientOcclusion : PER_VERTEX_AO;
        #if !defined(OUTPUT_PER_VERTEX_AO)
            #define OUTPUT_PER_VERTEX_AO 1
        #endif
    #endif

    VSINPUT_EXTRA
}; //////////////////////////////////////////////////////////////////

#if (MAT_DO_PARTICLE_LIGHTING==1) && (GEO_HAS_TEXCOORD==1) && (RES_HAS_NormalsTexture==1)
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
    #undef OUTPUT_LOCAL_TANGENT_FRAME

        // Can do this in either local or world space -- set OUTPUT_LOCAL_NORMAL & OUTPUT_LOCAL_VIEW_VECTOR for normal space
    #define OUTPUT_NORMAL 1
    #define OUTPUT_WORLD_VIEW_VECTOR 1
#endif

#if MAT_REFLECTIVENESS
    #define OUTPUT_WORLD_VIEW_VECTOR 1       // (need world view vector for the fresnel calculation)
#endif

#if MAT_DOUBLE_SIDED_LIGHTING
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
        float3 tangent : TEXTANGENT;
        float3 bitangent : TEXBITANGENT;
    #endif

    #if OUTPUT_LOCAL_TANGENT_FRAME==1
        float4 localTangent : LOCALTANGENT;
        float3 localBitangent : LOCALBITANGENT;
    #endif

    #if (OUTPUT_NORMAL==1)
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

    #if (OUTPUT_PRIMITIVE_ID==1)
        nointerpolation uint primitiveId : SV_PrimitiveID;
    #endif

    #if (OUTPUT_RENDER_TARGET_INDEX==1)
        nointerpolation uint renderTargetIndex : SV_RenderTargetArrayIndex;
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

    #if (OUTPUT_INSTANCE_ID==1)
        uint instanceId : SV_InstanceID;
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
    #endif

    #if (OUTPUT_SHADOW_PROJECTION_COUNT>0)
        uint shadowFrustumFlags : SHADOWFLAGS;
    #endif

    VSSHADOWOUTPUT_EXTRA
}; //////////////////////////////////////////////////////////////////


#endif
