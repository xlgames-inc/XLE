// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/DX11/Metal/DX11Utils.h"

namespace SceneEngine
{
    class PerlinNoiseResources
    {
    public:
        class Desc {};

        PerlinNoiseResources(const Desc& desc);
        ~PerlinNoiseResources();

        intrusive_ptr<ID3D::Resource>                          _gradTexture;
        RenderCore::Metal::ShaderResourceView               _gradShaderResource;

        intrusive_ptr<ID3D::Resource>                          _permTexture;
        RenderCore::Metal::ShaderResourceView               _permShaderResource;
    };
}

