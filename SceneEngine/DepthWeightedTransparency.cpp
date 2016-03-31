// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DepthWeightedTransparency.h"
#include "GestaltResource.h"
#include "LightingParserContext.h"
#include "SceneEngineUtils.h"
#include "../RenderCore/Metal/Format.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../Assets/Assets.h"

#include "../RenderCore/DX11/Metal/IncludeDX11.h"

namespace SceneEngine
{
    using namespace RenderCore;

    class DepthWeightedTransparencyBox
    {
    public:
        class Desc 
        {
        public:
            unsigned _width, _height;
            Desc(unsigned width, unsigned height)
            {
                _width = width;
                _height = height;
            }
        };
        GestaltTypes::RTVSRV    _accumulationBuffer;
        GestaltTypes::RTVSRV    _modulationBuffer;
        GestaltTypes::RTVSRV    _refractionBuffer;

        Metal::BlendState       _blendState;

        DepthWeightedTransparencyBox(const Desc& desc);
    };

    DepthWeightedTransparencyBox::DepthWeightedTransparencyBox(const Desc& desc)
    {
        using namespace BufferUploads;
        _accumulationBuffer = GestaltTypes::RTVSRV(
            TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R32G32B32A32_FLOAT),
            "TransAccBuffer", nullptr);
        _modulationBuffer = GestaltTypes::RTVSRV(
            TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R16G16B16A16_FLOAT),
            "TransModulationBuffer", nullptr);
        _refractionBuffer = GestaltTypes::RTVSRV(
            TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R16G16B16A16_FLOAT),
            "TransRefractBuffer", nullptr);

        D3D11_BLEND_DESC blendStateDesc;
        blendStateDesc.AlphaToCoverageEnable = false;
        blendStateDesc.IndependentBlendEnable = true;
        for (unsigned c=0; c<dimof(blendStateDesc.RenderTarget); ++c) {
            blendStateDesc.RenderTarget[c].BlendEnable = false;
            blendStateDesc.RenderTarget[c].SrcBlend = D3D11_BLEND_ONE;
            blendStateDesc.RenderTarget[c].DestBlend = D3D11_BLEND_ZERO;
            blendStateDesc.RenderTarget[c].BlendOp = D3D11_BLEND_OP_ADD;
            blendStateDesc.RenderTarget[c].SrcBlendAlpha = D3D11_BLEND_ONE;
            blendStateDesc.RenderTarget[c].DestBlendAlpha = D3D11_BLEND_ZERO;
            blendStateDesc.RenderTarget[c].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            blendStateDesc.RenderTarget[c].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }

        blendStateDesc.RenderTarget[0].BlendEnable = true;
        blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

        blendStateDesc.RenderTarget[1].BlendEnable = true;
        blendStateDesc.RenderTarget[1].SrcBlend = D3D11_BLEND_ZERO;
        blendStateDesc.RenderTarget[1].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
        blendStateDesc.RenderTarget[1].BlendOp = D3D11_BLEND_OP_ADD;
        blendStateDesc.RenderTarget[1].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendStateDesc.RenderTarget[1].DestBlendAlpha = D3D11_BLEND_ONE;
        blendStateDesc.RenderTarget[1].BlendOpAlpha = D3D11_BLEND_OP_ADD;

        blendStateDesc.RenderTarget[2].BlendEnable = true;
        blendStateDesc.RenderTarget[2].SrcBlend = D3D11_BLEND_ONE;
        blendStateDesc.RenderTarget[2].DestBlend = D3D11_BLEND_ONE;
        blendStateDesc.RenderTarget[2].BlendOp = D3D11_BLEND_OP_ADD;
        blendStateDesc.RenderTarget[2].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendStateDesc.RenderTarget[2].DestBlendAlpha = D3D11_BLEND_ONE;
        blendStateDesc.RenderTarget[2].BlendOpAlpha = D3D11_BLEND_OP_ADD;

        auto blendState = RenderCore::Metal::ObjectFactory().CreateBlendState(&blendStateDesc);
        _blendState = Metal::BlendState(std::move(blendState));
    }

    void DepthWeightedTransparencyOp::PrepareFirstPass(const Metal::DepthStencilView* dsv)
    {
        // We need to bind the render targets and blend modes for the initial accumulation pass
        // We can use a downsampled translucency buffer here -- when the loss of resolution doesn't
        // matter.
        Metal::ViewportDesc viewport(*_context);
        auto& box = Techniques::FindCachedBox2<DepthWeightedTransparencyBox>(
            unsigned(viewport.Width), unsigned(viewport.Height));

        _context->Clear(box._accumulationBuffer.RTV(), Float4(0.f, 0.f, 0.f, 0.f));
        _context->Clear(box._modulationBuffer.RTV(), Float4(1.f, 1.f, 1.f, 0.f));
        _context->Clear(box._refractionBuffer.RTV(), Float4(0.f, 0.f, 0.f, 0.f));
        _context->Bind(
            MakeResourceList(
                box._accumulationBuffer.RTV(),
                box._modulationBuffer.RTV(),
                box._refractionBuffer.RTV()), dsv);
        _context->Bind(box._blendState);
        _context->Bind(Techniques::CommonResources()._dssReadOnly);

        _box = &box;
    }

    void DepthWeightedTransparencyOp::Resolve()
    {
        if (!_box) return;

        {
            SetupVertexGeneratorShader(*_context);
            auto& shader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2d.vsh:fullscreen:vs_*",
                "game/xleres/forward/transparency/depthweighted.sh:resolve:ps_*");
            Metal::BoundUniforms uniforms(shader);
            Techniques::TechniqueContext::BindGlobalUniforms(uniforms);
            uniforms.BindShaderResources(1, {"Accumulator", "Modulator", "Refraction"});
            uniforms.Apply(
                *_context,
                _parserContext->GetGlobalUniformsStream(),
                Metal::UniformsStream(
                    {},
                    {
                        &_box->_accumulationBuffer.SRV(),
                        &_box->_modulationBuffer.SRV(),
                        &_box->_refractionBuffer.SRV()
                    }));

            _context->Bind(Techniques::CommonResources()._blendOneSrcAlpha);
            _context->Bind(shader);
            _context->Draw(4);
        }
    }

    DepthWeightedTransparencyOp::DepthWeightedTransparencyOp(
        RenderCore::Metal::DeviceContext& context, 
        LightingParserContext& parserContext) 
    : _context(&context)
    , _parserContext(&parserContext)
    , _box(nullptr)
    {}

    DepthWeightedTransparencyOp::~DepthWeightedTransparencyOp()
    {}
}


