// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StochasticTransparency.h"
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "GestaltResource.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/RenderTargetView.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../ConsoleRig/Console.h"
#include <tuple>
#include <type_traits>
#include <random>
#include <algorithm>

#include "../RenderCore/Assets/DeferredShaderResource.h"
#include "../Utility/StringFormat.h"
#include "MetricsBox.h"
#include "../RenderCore/DX11/Metal/DX11Utils.h"

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
            bool _recordMetrics;
            Desc(unsigned width, unsigned height, bool recordMetrics)
                : _width(width), _height(height), _recordMetrics(recordMetrics) {}
        };
        SRV     _masksTable;
        DSVSRV  _stochasticDepths;
        RTVSRV  _blendingTexture;

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
        auto masksTableData = BufferUploads::CreateBasicPacket(alphaValues*masksPerAlphaValue, nullptr, BufferUploads::TexturePitches(masksPerAlphaValue, alphaValues*masksPerAlphaValue));
        CreateCoverageMasks((uint8*)masksTableData->GetData(), masksTableData->GetDataSize(), alphaValues, masksPerAlphaValue);
        _masksTable = SRV(
            BufferUploads::TextureDesc::Plain2D(masksPerAlphaValue, alphaValues, Metal::NativeFormat::R8_UINT),
            "StochasticTransMasks", masksTableData.get());

        auto samples = BufferUploads::TextureSamples::Create(8, 0);
        _stochasticDepths = DSVSRV(
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::D32_FLOAT, 1, 0, samples),
            "StochasticDepths");

            // note --  perhaps we could re-use one of the deferred rendering MRT textures for this..?
            //          we may need 16 bit precision because this receives post-lit values 
        _blendingTexture = RTVSRV(
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R16G16B16A16_FLOAT),
            "StochasticBlendingTexture");

        _secondPassBlend = Metal::BlendState(
            Metal::BlendOp::Add, Metal::Blend::One, Metal::Blend::One,
            Metal::BlendOp::Add, Metal::Blend::Zero, Metal::Blend::InvSrcAlpha);

        if (desc._recordMetrics)
            _metricsTexture = UAVSRV(
                BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R32_UINT),
                "StochasticMetrics");
    }

    /// <summary>Low-level save and restore of state information</summary>
    /// 
    class ProtectState
    {
    public:
        struct States
        {
            enum Enum : unsigned
            {
                RenderTargets       = 1<<0,
                Viewports           = 1<<1,
                DepthStencilState   = 1<<2,
                BlendState          = 1<<3,
                Topology            = 1<<4,
                InputLayout         = 1<<5,
                VertexBuffer        = 1<<6,
                IndexBuffer         = 1<<7
            };
            using BitField = unsigned;
        };

        ProtectState(RenderCore::Metal::DeviceContext& context, States::BitField states);
        ~ProtectState();

    private:
        RenderCore::Metal::DeviceContext* _context;
        SavedTargets        _targets;
        States::BitField    _states;
        
        RenderCore::Metal::DepthStencilState    _depthStencilState;
        RenderCore::Metal::BoundInputLayout     _inputLayout;

        intrusive_ptr<ID3D::Buffer> _indexBuffer;
        DXGI_FORMAT _ibFormat;
        UINT        _ibOffset;

        static const auto s_vbCount = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
        intrusive_ptr<ID3D::Buffer> _vertexBuffers[s_vbCount];
        UINT        _vbStrides[s_vbCount];
        UINT        _vbOffsets[s_vbCount];

        intrusive_ptr<ID3D::BlendState> _blendState;
        float       _blendFactor[4];
        unsigned    _blendSampleMask;

        RenderCore::Metal::ViewportDesc _viewports;

        D3D11_PRIMITIVE_TOPOLOGY _topology;
    };

    ProtectState::ProtectState(RenderCore::Metal::DeviceContext& context, States::BitField states)
    : _context(&context), _states(states)
    {
        using namespace RenderCore::Metal;

        if (_states & States::RenderTargets || _states & States::Viewports)
            _targets = SavedTargets(context);
        if (_states & States::DepthStencilState)
            _depthStencilState = DepthStencilState(context);
        if (_states & States::BlendState) {
            ID3D::BlendState* rawptr = nullptr;
            context.GetUnderlying()->OMGetBlendState(&rawptr, _blendFactor, &_blendSampleMask);
            _blendState = moveptr(rawptr);
        }
        if (_states & States::InputLayout)
            _inputLayout = BoundInputLayout(context);

        if (_states & States::VertexBuffer) {
            ID3D::Buffer* rawptrs[s_vbCount];
            context.GetUnderlying()->IAGetVertexBuffers(0, s_vbCount, rawptrs, _vbStrides, _vbOffsets);
            for (unsigned c=0; c<s_vbCount; ++c)
                _vertexBuffers[c] = moveptr(rawptrs[c]);
        }

        if (_states & States::IndexBuffer) {
            ID3D::Buffer* rawptr = nullptr;
            context.GetUnderlying()->IAGetIndexBuffer(&rawptr, &_ibFormat, &_ibOffset);
            _indexBuffer = moveptr(rawptr);
        }

        if (_states & States::Topology) {
            context.GetUnderlying()->IAGetPrimitiveTopology(&_topology);
        }
    }

    ProtectState::~ProtectState()
    {
        if (_states & States::RenderTargets|| _states & States::Viewports)
            _targets.ResetToOldTargets(*_context);
        if (_states & States::DepthStencilState)
            _context->Bind(_depthStencilState);
        if (_states & States::BlendState)
            _context->GetUnderlying()->OMSetBlendState(_blendState.get(), _blendFactor, _blendSampleMask);
        if (_states & States::InputLayout)
            _context->Bind(_inputLayout);

        if (_states & States::VertexBuffer) {
            ID3D::Buffer* rawptrs[s_vbCount];
            for (unsigned c=0; c<s_vbCount; ++c)
                rawptrs[c] = _vertexBuffers[c].get();
            _context->GetUnderlying()->IASetVertexBuffers(0, s_vbCount, rawptrs, _vbStrides, _vbOffsets);
        }

        if (_states & States::IndexBuffer)
            _context->GetUnderlying()->IASetIndexBuffer(_indexBuffer.get(), _ibFormat, _ibOffset);
        if (_states & States::Topology)
            _context->GetUnderlying()->IASetPrimitiveTopology(_topology);
    }

    static void ShaderBasedCopy(
        RenderCore::Metal::DeviceContext& context,
        const Metal::DepthStencilView& dest,
        const Metal::ShaderResourceView& src,
        ProtectState::States::BitField protectStates = ~0u)
    {
        using States = ProtectState::States;
        const States::BitField effectedStates = 
            States::RenderTargets | States::Viewports | States::DepthStencilState 
            | States::Topology | States::InputLayout | States::VertexBuffer
            ;
        ProtectState savedStates(context, effectedStates & protectStates);

        context.Bind(ResourceList<Metal::RenderTargetView, 0>(), &dest);
        context.Bind(Techniques::CommonResources()._dssWriteOnly);
        context.Bind(
            ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2d.vsh:fullscreen:vs_*",
                "game/xleres/basic.psh:copy_depth:ps_*"));
        context.BindPS(MakeResourceList(src));
        SetupVertexGeneratorShader(context);
        context.Draw(4);
    }

    static void RenderGPUMetrics(
        RenderCore::Metal::DeviceContext& context,
        LightingParserContext& parsingContext,
        const ::Assets::ResChar shaderName[],
        std::initializer_list<const ::Assets::ResChar*> valueSources,
        ProtectState::States::BitField protectStates = ~0u)
    {
        using States = ProtectState::States;
        const States::BitField effectedStates = 
            States::DepthStencilState | States::BlendState
            | States::Topology | States::InputLayout | States::VertexBuffer
            ;
        ProtectState savedStates(context, effectedStates & protectStates);

            // Utility function for writing GPU metrics values to the screen.
            // This is useful when we have metrics values that don't reach the
            // CPU. Ie, they are written to UAV resources (or other resources)
            // and then read back on the GPU during the same frame. The geometry
            // shader converts the numbers into a string, and textures appropriately.
            // So a final value can be written to the screen without ever touching
            // the CPU.
        const auto& shader = ::Assets::GetAssetDep<Metal::DeepShaderProgram>(
            (StringMeld<MaxPath, ::Assets::ResChar>() << shaderName << ":metricsrig_main:!vs_*").get(),
            "game/xleres/utility/metricsrender.gsh:main:gs_*",
            "game/xleres/utility/metricsrender.psh:main:ps_*",
            "", "", 
            (StringMeld<64>() << "VALUE_SOURCE_COUNT=" << valueSources.size()).get());
        Metal::BoundClassInterfaces boundInterfaces(shader);
        for (unsigned c=0; c<valueSources.size(); ++c)
            boundInterfaces.Bind(Hash64("ValueSource"), c, valueSources.begin()[c]);
        context.Bind(shader, boundInterfaces);

        const auto* metricsDigits = "game/xleres/DefaultResources/metricsdigits.dds:T";
        context.BindPS(MakeResourceList(3, ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>(metricsDigits).GetShaderResource()));

        Metal::BoundUniforms uniforms(shader);
        Techniques::TechniqueContext::BindGlobalUniforms(uniforms);
        uniforms.BindConstantBuffers(1, {"$Globals"});
        uniforms.BindShaderResources(1, {"MetricsObject"});

        Metal::ViewportDesc viewport(context);
        unsigned globalCB[4] = { unsigned(viewport.Width), unsigned(viewport.Height), 0, 0 };
        uniforms.Apply(
            context, parsingContext.GetGlobalUniformsStream(),
            Metal::UniformsStream(
                { MakeSharedPkt(globalCB) }, 
                { &parsingContext.GetMetricsBox()->_metricsBufferSRV }));

        context.Unbind<Metal::VertexBuffer>();
        context.Unbind<Metal::BoundInputLayout>();
        context.Bind(Metal::Topology::PointList);
        context.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
        context.Bind(Techniques::CommonResources()._dssDisable);
        context.Draw((unsigned)valueSources.size());

        uniforms.UnbindShaderResources(context, 1);
    }

    StochasticTransparencyBox* StochasticTransparency_Prepare(
        RenderCore::Metal::DeviceContext& context, 
        LightingParserContext& parserContext,
        Metal::ShaderResourceView& mainDSV)
    {
            // Bind the resources we'll need for the initial passes
        Metal::ViewportDesc viewport(context);
        #if defined(_DEBUG)
            const auto enableMetrics = true;
        #else
            const auto enableMetrics = false;
        #endif
        auto& box = Techniques::FindCachedBox2<StochasticTransparencyBox>(
            unsigned(viewport.Width), unsigned(viewport.Height), enableMetrics);

            // Copy the main depth buffer onto the multisample depth buffer
            // we'll be using for the stochastic depths. This should help
            // by rejecting samples that are occluded by opaque geometry.
        const bool copyMainDepths = Tweakable("Stochastic_CopyMainDepths", true);
        if (copyMainDepths) {
            using States = ProtectState::States;
            ShaderBasedCopy(
                context, box._stochasticDepths.DSV(), mainDSV, 
                ~(States::RenderTargets | States::Viewports));
        } else {
            context.Clear(box._stochasticDepths.DSV(), 1.f, 0u);    // (if we don't copy any opaque depths, just clear)
        }

        context.BindPS(MakeResourceList(18, box._masksTable.SRV()));
        context.Bind(ResourceList<Metal::RenderTargetView, 0>(), &box._stochasticDepths.DSV());
        context.Bind(Techniques::CommonResources()._dssReadWrite);

        return &box;
    }

    void StochasticTransparencyBox_PrepareSecondPass(  
        RenderCore::Metal::DeviceContext& context,
        LightingParserContext& parserContext,
        StochasticTransparencyBox& box,
        Metal::DepthStencilView& mainDSV)
    {
        float clearValues[] = {0.f, 0.f, 0.f, 1.f};
        context.Clear(box._blendingTexture.RTV(), clearValues); 
        if (box._metricsTexture.IsGood()) {
            unsigned uavClear[] = {0,0,0,0};
            context.Clear(box._metricsTexture.UAV(), uavClear);
            context.Bind(
                MakeResourceList(box._blendingTexture.RTV()), &mainDSV, 
                MakeResourceList(parserContext.GetMetricsBox()->_metricsBufferUAV, box._metricsTexture.UAV()));
        } else
            context.Bind(MakeResourceList(box._blendingTexture.RTV()), &mainDSV);
        context.BindPS(MakeResourceList(9, box._stochasticDepths.SRV()));
        context.Bind(box._secondPassBlend);
        context.Bind(Techniques::CommonResources()._dssReadOnly);
    }

    void StochasticTransparencyBox_Resolve(  
        RenderCore::Metal::DeviceContext& context,
        LightingParserContext& parserContext,
        StochasticTransparencyBox& box)
    {
        context.UnbindPS<Metal::ShaderResourceView>(9, 1); // unbind box._stochasticDepths

        {
            SetupVertexGeneratorShader(context);
            auto& shader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2d.vsh:fullscreen:vs_*",
                "game/xleres/basic.psh:copy:ps_*");
            Metal::BoundUniforms uniforms(shader);
            Techniques::TechniqueContext::BindGlobalUniforms(uniforms);
            uniforms.BindShaderResources(1, {"DiffuseTexture"});
            uniforms.Apply(
                context, parserContext.GetGlobalUniformsStream(),
                Metal::UniformsStream({}, {&box._blendingTexture.SRV()}));

            context.Bind(Techniques::CommonResources()._dssDisable);
            if (Tweakable("Stochastic_BlendBuffer", true)) {
                context.Bind(Techniques::CommonResources()._blendOneSrcAlpha);
            } else {
                context.Bind(Techniques::CommonResources()._blendOpaque);
            }
            context.Bind(shader);
            context.Draw(4);
        }

        if (Tweakable("StochTransMetrics", false) && box._metricsTexture.IsGood()) {
            parserContext._pendingOverlays.push_back(
                [&box](RenderCore::Metal::DeviceContext& context, LightingParserContext& parserContext)
                {
                    auto& shader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                        "game/xleres/basic2d.vsh:fullscreen:vs_*",
                        "game/xleres/forward/transparency/stochasticdebug.sh:ps_pixelmetrics:ps_*");
                    Metal::BoundUniforms uniforms(shader);
                    Techniques::TechniqueContext::BindGlobalUniforms(uniforms);
                    uniforms.BindShaderResources(1, {"DepthsTexture", "LitSamplesMetrics"});
                    uniforms.Apply(
                        context, parserContext.GetGlobalUniformsStream(),
                        Metal::UniformsStream({}, {&box._stochasticDepths.SRV(), &box._metricsTexture.SRV()}));

                    context.Bind(shader);
                    context.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
                    context.Draw(4);

                    RenderGPUMetrics(
                        context, parserContext,
                        "game/xleres/forward/transparency/stochasticdebug.sh",
                        {"LitFragmentCount", "AveLitFragment"});
                });
        }

        if (Tweakable("StochTransDebug", false)) {
            auto& shader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2d.vsh:fullscreen:vs_*",
                "game/xleres/forward/transparency/stochasticdebug.sh:ps_depthave:ps_*");
            Metal::BoundUniforms uniforms(shader);
            Techniques::TechniqueContext::BindGlobalUniforms(uniforms);
            uniforms.BindShaderResources(1, {"DepthsTexture"});
            uniforms.Apply(
                context, parserContext.GetGlobalUniformsStream(),
                Metal::UniformsStream({}, {&box._stochasticDepths.SRV()}));

            context.Bind(shader);
            context.Draw(4);
        }
    }

}


