// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StochasticTransparency.h"
#include "SceneEngineUtils.h"
#include "GestaltResource.h"
#include "MetricsBox.h"
#include "MetalStubs.h"
#include "../BufferUploads/DataPacket.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/TextureView.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Format.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../ConsoleRig/Console.h"
#include "../xleres/FileList.h"
#include <tuple>
#include <type_traits>
#include <random>
#include <algorithm>

#include "../RenderCore/DX11/Metal/DeviceContextImpl.h"

namespace SceneEngine
{
    using namespace RenderCore;

///////////////////////////////////////////////////////////////////////////////////////////////////
    
    using namespace GestaltTypes;

    class StochasticTransparencyBox
    {
    public:
        class Desc 
        {
        public:
            unsigned _width, _height;
            bool _recordMetrics, _recordPrimIds;
            bool _recordOpacity;
            Desc(unsigned width, unsigned height, bool recordPrimIds, bool recordOpacity, bool recordMetrics)
            {
                XlZeroMemory(*this);
                _width = width;
                _height = height;
                _recordPrimIds = recordPrimIds;
                _recordMetrics = recordMetrics;
                _recordOpacity = recordOpacity;
            }
        };
        SRV     _masksTable;
        DSVSRV  _stochasticDepths;
        RTVSRV  _blendingTexture;

        RTVSRV  _opacitiesTexture;
        RTVSRV  _primIdsTexture;
        UAVSRV  _metricsTexture;

        Metal::BlendState _secondPassBlend;

        StochasticTransparencyBox(const Desc& desc);
    };

    static void CreateCoverageMasks(
        uint8 destination[], size_t destinationBytes,
        unsigned alphaValues, unsigned masksPerAlphaValue)
    {
        // Create a series of coverage masks for possible alpha values
        // The masks determine which samples are written to in the MSAA buffer
        // For example, if our alpha value is 30%, we want to write to approximately
        // 30% of the samples.
        //
        // But imagine that there is another layer of 30% transparency on top -- it will
        // also write to 30% of the samples. Which samples are selected is important: 
        // depending on what samples overlap for the 2 layers, the result will be different.
        //
        // Each mask calculated here is just a random selection of samples. The randomness
        // adds noise, but it means that the image will gradually coverge on the right result.
        //
        // We will quantize alpha values down to a limited resolution, and for each unique 
        // alpha value we will calculate a number of different coverage masks.
        //
        // Note -- this function is a little expensive to run. We should precalculate this
        // texture and just load it from disk.

        std::mt19937 rng(0);
        for (unsigned y=0; y<alphaValues; y++) {
            for (unsigned x=0; x<masksPerAlphaValue; x++) {
                const auto maskSize = 8u;
                unsigned numbers[maskSize];
                for (unsigned i=0; i<dimof(numbers); i++)
                    numbers[i] = i;

                std::shuffle(numbers, &numbers[dimof(numbers)], rng);
                std::shuffle(numbers, &numbers[dimof(numbers)], rng);

                    // Create the mask
                    // derived from DX sample by Eric Enderton
                    // This will create purely random masks.
                unsigned int mask = 0;
                auto setBitCount = (float(y) / float(alphaValues-1)) * float(maskSize);
                for (int bit = 0; bit < int(setBitCount); bit++)
                    mask |= (1 << numbers[bit]);

                    // since we floor above, the last bit will only be set in some of the masks
                float prob_of_last_bit = (setBitCount - XlFloor(setBitCount));
                if (std::uniform_real_distribution<>(0, 1.f)(rng) < prob_of_last_bit)
                    mask |= (1 << numbers[int(setBitCount)]);

                assert(((y * masksPerAlphaValue + x)+1)*sizeof(uint8) <= destinationBytes);
                destination[y * masksPerAlphaValue + x] = (uint8)mask;
            }
        }
    }

    StochasticTransparencyBox::StochasticTransparencyBox(const Desc& desc)
    {
        const auto alphaValues = 256u;
        const auto masksPerAlphaValue = 2048u;
        auto masksTableData = BufferUploads::CreateBasicPacket(alphaValues*masksPerAlphaValue, nullptr, BufferUploads::TexturePitches{masksPerAlphaValue, alphaValues*masksPerAlphaValue});
        CreateCoverageMasks((uint8*)masksTableData->GetData(), masksTableData->GetDataSize(), alphaValues, masksPerAlphaValue);
        _masksTable = SRV(
            BufferUploads::TextureDesc::Plain2D(masksPerAlphaValue, alphaValues, Format::R8_UINT),
            "StochasticTransMasks", masksTableData.get());

        auto samples = BufferUploads::TextureSamples::Create(8, 0);
        _stochasticDepths = DSVSRV(
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Format::D32_FLOAT, 1, 0, samples),
            "StochasticDepths");

            // note --  perhaps we could re-use one of the deferred rendering MRT textures for this..?
            //          we may need 16 bit precision because this receives post-lit values 
        _blendingTexture = RTVSRV(
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Format::R16G16B16A16_FLOAT),
            "StochasticBlendingTexture");

        _secondPassBlend = Metal::BlendState(
            BlendOp::Add, Blend::One, Blend::One,
            BlendOp::Add, Blend::Zero, Blend::InvSrcAlpha);

        if (desc._recordPrimIds)
            _primIdsTexture = RTVSRV(
                BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Format::R32_UINT, 1, 0, samples),
                "StochasticPrimitiveIds");

        if (desc._recordOpacity)
            _opacitiesTexture = RTVSRV(
                BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Format::R8_UNORM, 1, 0, samples),
                "StochasticOpacities");

        if (desc._recordMetrics)
            _metricsTexture = UAVSRV(
                BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Format::R32_UINT),
                "StochasticMetrics");
    }

    StochasticTransparencyOp::StochasticTransparencyOp(
        RenderCore::Metal::DeviceContext& context, 
        Techniques::ParsingContext& parserContext) 
    : _context(&context)
    , _parserContext(&parserContext)
    , _box(nullptr)
    {}

    StochasticTransparencyOp::~StochasticTransparencyOp() 
    {
        if (_parserContext) {
            auto& runtimeState = _parserContext->GetSubframeShaderSelectors();
            runtimeState.SetParameter(u("STOCHASTIC_TRANS_PRIMITIVEID"), 0u);
            runtimeState.SetParameter(u("STOCHASTIC_TRANS_OPACITY"), 0u);
            runtimeState.SetParameter(u("STOCHASTIC_TRANS"), 1u);
        }
    }

    void StochasticTransparencyOp::PrepareFirstPass(const Metal::ShaderResourceView& mainDSV)
    {
            // Bind the resources we'll need for the initial passes
        Metal::ViewportDesc viewport = _context->GetBoundViewport();
        #if defined(_DEBUG)
            const auto enableMetrics = true;
        #else
            const auto enableMetrics = false;
        #endif
        const auto enablePrimitiveIds = Tweakable("Stochastic_PrimIds", false);
        const auto enableOpacity = Tweakable("Stochastic_Opacity", false);
        auto& box = ConsoleRig::FindCachedBox2<StochasticTransparencyBox>(
            unsigned(viewport.Width), unsigned(viewport.Height), 
            enablePrimitiveIds, enableOpacity, enableMetrics);

            // Copy the main depth buffer onto the multisample depth buffer
            // we'll be using for the stochastic depths. This should help
            // by rejecting samples that are occluded by opaque geometry.
        const bool copyMainDepths = Tweakable("Stochastic_CopyMainDepths", true);
        if (copyMainDepths) {
            using States = ProtectState::States;
            ShaderBasedCopy(
                *_context, box._stochasticDepths.DSV(), mainDSV, 
                ~(States::RenderTargets | States::Viewports));
        } else {
            _context->Clear(box._stochasticDepths.DSV(), Metal::DeviceContext::ClearFilter::Depth|Metal::DeviceContext::ClearFilter::Stencil, 1.f, 0u);    // (if we don't copy any opaque depths, just clear)
        }

        _context->GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(18, box._masksTable.SRV()));
        _context->Bind(Techniques::CommonResources()._dssReadWrite);
        _context->Bind(Techniques::CommonResources()._blendOpaque);

        auto& runtimeState = _parserContext->GetSubframeShaderSelectors();
        runtimeState.SetParameter(u("STOCHASTIC_TRANS_PRIMITIVEID"), box._primIdsTexture.IsGood());
        runtimeState.SetParameter(u("STOCHASTIC_TRANS_OPACITY"), box._opacitiesTexture.IsGood());
        runtimeState.SetParameter(u("STOCHASTIC_TRANS"), 1u);

            // do we need to clear the opacity and primitive ids textures? Maybe it's ok
        if (box._primIdsTexture.IsGood()) {
            if (box._opacitiesTexture.IsGood()) {
                _context->Bind(MakeResourceList(box._primIdsTexture.RTV(), box._opacitiesTexture.RTV()), &box._stochasticDepths.DSV());
            } else 
                _context->Bind(MakeResourceList(box._primIdsTexture.RTV()), &box._stochasticDepths.DSV());
        } else if (box._opacitiesTexture.IsGood()) {
            _context->Bind(MakeResourceList(box._opacitiesTexture.RTV()), &box._stochasticDepths.DSV());
        } else {
            _context->Bind(ResourceList<Metal::RenderTargetView, 0>(), &box._stochasticDepths.DSV());
        }

        _box = &box;
    }

    void StochasticTransparencyOp::PrepareSecondPass(Metal::DepthStencilView& mainDSV, const MetricsBox& metricsBox)
    {
        if (!_box) return;

        _context->Clear(_box->_blendingTexture.RTV(), { 0.f, 0.f, 0.f, 1.f });
        if (_box->_metricsTexture.IsGood()) {
            _context->ClearUInt(_box->_metricsTexture.UAV(), { 0,0,0,0 });
            _context->Bind(
                MakeResourceList(_box->_blendingTexture.RTV()), &mainDSV, 
                MakeResourceList(metricsBox._metricsBufferUAV, _box->_metricsTexture.UAV()));
        } else
            _context->Bind(MakeResourceList(_box->_blendingTexture.RTV()), &mainDSV);
        if (_box->_primIdsTexture.IsGood()) _context->GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(8, _box->_primIdsTexture.SRV()));
        if (_box->_opacitiesTexture.IsGood()) _context->GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(7, _box->_opacitiesTexture.SRV()));
        _context->GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(9, _box->_stochasticDepths.SRV()));
        _context->Bind(_box->_secondPassBlend);
        _context->Bind(Techniques::CommonResources()._dssReadOnly);
    }

    void StochasticTransparencyOp::Resolve()
    {
        if (!_box) return;

        MetalStubs::UnbindPS<Metal::ShaderResourceView>(*_context, 7, 3); // unbind box._stochasticDepths, opacities texture, prim ids texture

        {
            SetupVertexGeneratorShader(*_context);
            auto& shader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                BASIC2D_VERTEX_HLSL ":fullscreen:vs_*",
                BASIC_PIXEL_HLSL ":copy:ps_*");
			UniformsStreamInterface usi;
			usi.BindShaderResource(0, Hash64("DiffuseTexture"));
            Metal::BoundUniforms uniforms(
				shader,
				Metal::PipelineLayoutConfig{},
				Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
				usi);
            uniforms.Apply(*_context, 0, _parserContext->GetGlobalUniformsStream());
			const Metal::ShaderResourceView* srvs[] = {&_box->_blendingTexture.SRV()};
			uniforms.Apply(*_context, 1, UniformsStream{{}, UniformsStream::MakeResources(MakeIteratorRange(srvs))});

            _context->Bind(Techniques::CommonResources()._dssDisable);
            _context->Bind(Tweakable("Stochastic_BlendBuffer", true)
                ? Techniques::CommonResources()._blendOneSrcAlpha
                : Techniques::CommonResources()._blendOpaque);
            _context->Bind(shader);
            _context->Draw(4);
        }

        if (Tweakable("StochTransMetrics", false) && _box->_metricsTexture.IsGood()) {
            auto* box = _box;
            _parserContext->_pendingOverlays.push_back(
                [box](RenderCore::Metal::DeviceContext& context, Techniques::ParsingContext& parserContext)
                {
                    auto& shader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                        BASIC2D_VERTEX_HLSL ":fullscreen:vs_*",
                        "xleres/forward/transparency/stochasticdebug.hlsl:ps_pixelmetrics:ps_*");
					UniformsStreamInterface usi;
					usi.BindShaderResource(0, Hash64("DepthsTexture"));
					usi.BindShaderResource(1, Hash64("LitSamplesMetrics"));
                    Metal::BoundUniforms uniforms(
						shader,
						Metal::PipelineLayoutConfig{},
						Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
						usi);
                    uniforms.Apply(context, 0, parserContext.GetGlobalUniformsStream());
					const Metal::ShaderResourceView* srvs[] = {&box->_stochasticDepths.SRV(), &box->_metricsTexture.SRV()};
					uniforms.Apply(context, 1, UniformsStream{{}, UniformsStream::MakeResources(MakeIteratorRange(srvs))});

                    context.Bind(shader);
                    context.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
                    context.Draw(4);

                    RenderGPUMetrics(
                        context, parserContext,
						MetricsBox{MetricsBox::Desc{}},
                        "xleres/forward/transparency/stochasticdebug.hlsl",
                        {"LitFragmentCount", "AveLitFragment", "PartialLitFragment"});
                });
        }

        if (Tweakable("StochTransDebug", false)) {
            auto& shader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                BASIC2D_VERTEX_HLSL ":fullscreen:vs_*",
                "xleres/forward/transparency/stochasticdebug.hlsl:ps_depthave:ps_*");
			UniformsStreamInterface usi;
			usi.BindShaderResource(0, Hash64("DepthsTexture"));
            Metal::BoundUniforms uniforms(
				shader,
				Metal::PipelineLayoutConfig{},
				Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
				usi);
            uniforms.Apply(*_context, 0, _parserContext->GetGlobalUniformsStream());
			const Metal::ShaderResourceView* srvs[] = {&_box->_stochasticDepths.SRV()};
			uniforms.Apply(*_context, 1, UniformsStream{{}, UniformsStream::MakeResources(MakeIteratorRange(srvs))});

            _context->Bind(shader);
            _context->Draw(4);
        }
    }

}


