// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#undef OUTPUT_COLOUR
#undef OUTPUT_LOCAL_TANGENT_FRAME
#undef OUTPUT_TANGENT_FRAME
#undef OUTPUT_NORMAL
#undef OUTPUT_RENDER_TARGET_INDEX

#define OUTPUT_COLOUR 0
#define OUTPUT_LOCAL_TANGENT_FRAME 0
#define OUTPUT_TANGENT_FRAME 0
#define OUTPUT_NORMAL 0
#if (MAT_ALPHA_TEST!=1) && (MAT_ALPHA_DITHER_SHADOWS!=1)
	#undef GEO_HAS_TEXCOORD
	#define GEO_HAS_TEXCOORD 0
#endif
#define OUTPUT_RENDER_TARGET_INDEX 1

//#if (GEO_HAS_TEXCOORD==1) && (MAT_ALPHA_DITHER_SHADOWS==1)
//	#define OUTPUT_PRIMITIVE_ID 1
//#endif

#if !defined(OUTPUT_SHADOW_PROJECTION_COUNT)
	#define OUTPUT_SHADOW_PROJECTION_COUNT 6
#endif

// Normal is only required when wind animation is enabled
// So disable when we can
#if !((MAT_VCOLOR_IS_ANIM_PARAM!=0) && (GEO_HAS_COLOR!=0))
	#undef GEO_HAS_NORMAL
	#define GEO_HAS_NORMAL 1
#endif
