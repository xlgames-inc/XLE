// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(COMMON_RESOURCES_H)
#define COMMON_RESOURCES_H

#include "Binding.h"

Texture2D		DiffuseTexture          : TEXTURE_BOUND(0);
Texture2D		NormalsTexture          : TEXTURE_BOUND(1);
Texture2D       ParametersTexture       : TEXTURE_BOUND(2);

SamplerState	DefaultSampler          : SAMPLER_GLOBAL(0);
SamplerState	ClampingSampler         : SAMPLER_GLOBAL(1);
SamplerState    AnisotropicSampler      : SAMPLER_GLOBAL(2);
SamplerState    PointClampSampler       : SAMPLER_GLOBAL(3);
SamplerState    WrapUSampler            : SAMPLER_GLOBAL(6);

Texture2D       NormalsFittingTexture   : TEXTURE_GLOBAL(2);

#if !defined(PREFER_ANISOTROPIC) || PREFER_ANISOTROPIC==0
    #define MaybeAnisotropicSampler   DefaultSampler
#else
    #define MaybeAnisotropicSampler   AnisotropicSampler
#endif

#endif
