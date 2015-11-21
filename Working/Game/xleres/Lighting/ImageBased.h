// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(IMAGE_BASED_H)
#define IMAGE_BASED_H

#include "../CommonResources.h"

TextureCube DiffuseIBL : register(t19);

float3 SampleDiffuseIBL(float3 worldSpaceNormal)
{
    #if HAS_DIFFUSE_IBL==1
        return DiffuseIBL.SampleLevel(DefaultSampler, worldSpaceNormal, 0);
    #else
        return 0.0.xxx;
    #endif
}

#endif
