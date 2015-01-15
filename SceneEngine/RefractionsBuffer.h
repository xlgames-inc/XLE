// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/RenderTargetView.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/Format.h"
#include "../RenderCore/Metal/Forward.h"
#include "../BufferUploads/IBufferUploads.h"

namespace SceneEngine
{
    class LightingParserContext;

    class RefractionsBuffer
    {
    public:
        class Desc
        {
        public:
            Desc(unsigned width, unsigned height);
            unsigned _width, _height;
        };

        RefractionsBuffer(const Desc& desc);
        ~RefractionsBuffer();

        intrusive_ptr<ID3D::Resource>              _refractionsTexture[2];
        RenderCore::Metal::RenderTargetView     _refractionsFrontTarget;
        RenderCore::Metal::RenderTargetView     _refractionsBackTarget;
        RenderCore::Metal::ShaderResourceView   _refractionsFrontSRV;
        RenderCore::Metal::ShaderResourceView   _refractionsBackSRV;
        unsigned _width, _height;
    };

    inline RefractionsBuffer::Desc::Desc(unsigned width, unsigned height) { _width = width; _height = height; }

    void        BuildRefractionsTexture(RenderCore::Metal::DeviceContext* context, 
                                        LightingParserContext& parserContext,
                                        RefractionsBuffer& refractionBox, float standardDeviationForBlur);



    class DuplicateDepthBuffer
    {
    public:
        class Desc
        {
        public:
            Desc(   unsigned width, unsigned height, 
                    RenderCore::Metal::NativeFormat::Enum format, 
                    const BufferUploads::TextureSamples& samping);
            unsigned _width, _height;
            RenderCore::Metal::NativeFormat::Enum _format;
            BufferUploads::TextureSamples _sampling;
        };

        DuplicateDepthBuffer(const Desc& desc);
        ~DuplicateDepthBuffer();

        intrusive_ptr<ID3D::Resource>              _resource;
        RenderCore::Metal::ShaderResourceView   _srv;
    };

    RenderCore::Metal::ShaderResourceView BuildDuplicatedDepthBuffer(
        RenderCore::Metal::DeviceContext* context, ID3D::Resource* sourceDepthBuffer);

}

