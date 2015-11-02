// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/ShaderResource.h"

namespace SceneEngine
{
    class PerlinNoiseResources
    {
    public:
        class Desc {};

        PerlinNoiseResources(const Desc& desc);
        ~PerlinNoiseResources();

        using SRV = RenderCore::Metal::ShaderResourceView;
        SRV _gradShaderResource;
        SRV _permShaderResource;
    };
}

