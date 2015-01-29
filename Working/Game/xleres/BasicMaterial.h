// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(BASIC_MATERIAL_H)
#define BASIC_MATERIAL_H

    //  This cbuffer contains basic constants are used frequently enough that 
    //  we can add support for them in most shaders.
cbuffer BasicMaterialConstants
{
	float3  MaterialDiffuse;
    float   Opacity;
    float3  MaterialSpecular;
    float   AlphaThreshold;
}

#endif
