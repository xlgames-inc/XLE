// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#undef GEO_HAS_COLOUR
#if (MAT_ALPHA_TEST!=1)
	#undef GEO_HAS_TEXCOORD
#endif
#define OUTPUT_RENDER_TARGET_INDEX 1
#define OUTPUT_SHADOW_PROJECTION_COUNT 6
