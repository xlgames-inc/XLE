// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(COMMON_RESOURCES_H)
#define COMMON_RESOURCES_H

#include "../Framework/Binding.hlsl"

SamplerState	DefaultSampler          BIND_NUMERIC_S0;
SamplerState	ClampingSampler         BIND_NUMERIC_S1;
SamplerState    AnisotropicSampler      BIND_NUMERIC_S2;
SamplerState    PointClampSampler       BIND_NUMERIC_S3;
SamplerState    WrapUSampler            BIND_NUMERIC_S6;

#if !defined(PREFER_ANISOTROPIC) || PREFER_ANISOTROPIC==0
    #define MaybeAnisotropicSampler   DefaultSampler
#else
    #define MaybeAnisotropicSampler   AnisotropicSampler
#endif

#endif
