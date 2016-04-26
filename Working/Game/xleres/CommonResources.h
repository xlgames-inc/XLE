// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(COMMON_RESOURCES_H)
#define COMMON_RESOURCES_H

#include "Binding.h"

SamplerState	DefaultSampler          : SAMPLER_GLOBAL_0;
SamplerState	ClampingSampler         : SAMPLER_GLOBAL_1;
SamplerState    AnisotropicSampler      : SAMPLER_GLOBAL_2;
SamplerState    PointClampSampler       : SAMPLER_GLOBAL_3;
SamplerState    WrapUSampler            : SAMPLER_GLOBAL_6;

Texture2D       NormalsFittingTexture   : TEXTURE_GLOBAL_2;

#if !defined(PREFER_ANISOTROPIC) || PREFER_ANISOTROPIC==0
    #define MaybeAnisotropicSampler   DefaultSampler
#else
    #define MaybeAnisotropicSampler   AnisotropicSampler
#endif

#endif
