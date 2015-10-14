// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SceneEngineUtils.h"

#include "LightingParserContext.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Assets/Services.h"
#include "../RenderCore/Assets/DelayedDrawCall.h"
#include "../RenderOverlays/Font.h"
#include "../Assets/Assets.h"
#include "../Utility/IteratorUtils.h"

#include "../RenderCore/DX11/Metal/IncludeDX11.h"
#include "../RenderCore/DX11/Metal/DX11Utils.h" // for TextureDesc

namespace SceneEngine
{
    using namespace RenderCore;

    BufferUploads::IManager& GetBufferUploads()
    {
        return RenderCore::Assets::Services::GetBufferUploads();
    }

    SavedTargets::SavedTargets(Metal::DeviceContext& context)
    {
        _oldViewportCount = dimof(_oldViewports);
        std::fill(_oldTargets, &_oldTargets[dimof(_oldTargets)], nullptr);
        context.GetUnderlying()->OMGetRenderTargets(dimof(_oldTargets), _oldTargets, &_oldDepthTarget);
        context.GetUnderlying()->RSGetViewports(&_oldViewportCount, (D3D11_VIEWPORT*)_oldViewports);
    }

    SavedTargets::SavedTargets()
    {
        _oldViewportCount = 0;
        std::fill(_oldTargets, &_oldTargets[dimof(_oldTargets)], nullptr);
        _oldDepthTarget = nullptr;
    }

    SavedTargets::SavedTargets(SavedTargets&& moveFrom) never_throws
    {
        _oldViewportCount = moveFrom._oldViewportCount; moveFrom._oldViewportCount = 0;
        for (unsigned c=0; c<MaxSimultaneousRenderTargetCount; ++c) {
            _oldTargets[c] = moveFrom._oldTargets[c];
            _oldViewports[c] = moveFrom._oldViewports[c];
            moveFrom._oldTargets[c] = nullptr;
            moveFrom._oldViewports[c] = Metal::ViewportDesc();
        }
        _oldDepthTarget = moveFrom._oldDepthTarget; moveFrom._oldDepthTarget = nullptr;
    }

    SavedTargets& SavedTargets::operator=(SavedTargets&& moveFrom) never_throws
    {
        _oldViewportCount = moveFrom._oldViewportCount; moveFrom._oldViewportCount = 0;
        for (unsigned c=0; c<MaxSimultaneousRenderTargetCount; ++c) {
            _oldTargets[c] = moveFrom._oldTargets[c];
            _oldViewports[c] = moveFrom._oldViewports[c];
            moveFrom._oldTargets[c] = nullptr;
            moveFrom._oldViewports[c] = Metal::ViewportDesc();
        }
        _oldDepthTarget = moveFrom._oldDepthTarget; moveFrom._oldDepthTarget = nullptr;
        return *this;
    }

    SavedTargets::~SavedTargets()
    {
         for (unsigned c=0; c<dimof(_oldTargets); ++c) {
            if (_oldTargets[c]) {
                _oldTargets[c]->Release();
            }
        }
        if (_oldDepthTarget) {
            _oldDepthTarget->Release();
        }
    }

    void SavedTargets::SetDepthStencilView(ID3D::DepthStencilView* dsv)
    {
        if (_oldDepthTarget) {
            _oldDepthTarget->Release();
        }
        _oldDepthTarget = dsv;
        _oldDepthTarget->AddRef();
    }

    void        SavedTargets::ResetToOldTargets(Metal::DeviceContext& context)
    {
        context.GetUnderlying()->OMSetRenderTargets(dimof(_oldTargets), _oldTargets, _oldDepthTarget);
        context.GetUnderlying()->RSSetViewports(_oldViewportCount, (D3D11_VIEWPORT*)_oldViewports);
    }

    void SavedBlendAndRasterizerState::ResetToOldStates(Metal::DeviceContext& context)
    {
        context.GetUnderlying()->RSSetState(_oldRasterizerState.get());
        context.GetUnderlying()->OMSetBlendState(_oldBlendState.get(), _oldBlendFactor, _oldSampleMask);
    }

    SavedBlendAndRasterizerState::SavedBlendAndRasterizerState(Metal::DeviceContext& context)
    {
        ID3D::RasterizerState* rs = nullptr;
        ID3D::BlendState* bs = nullptr;
        context.GetUnderlying()->RSGetState(&rs);
        context.GetUnderlying()->OMGetBlendState(&bs, _oldBlendFactor, &_oldSampleMask);

        _oldRasterizerState = intrusive_ptr<ID3D::RasterizerState>(rs, false);
        _oldBlendState = intrusive_ptr<ID3D::BlendState>(bs, false);
    }

    SavedBlendAndRasterizerState::~SavedBlendAndRasterizerState() {}

    BufferUploads::BufferDesc BuildRenderTargetDesc( 
        BufferUploads::BindFlag::BitField bindFlags, 
        const BufferUploads::TextureDesc& textureDesc,
        const char name[])
    {
        using namespace BufferUploads;
        return CreateDesc(
            bindFlags, 0, GPUAccess::Read|GPUAccess::Write,
            textureDesc, name);
    }

    void SetupVertexGeneratorShader(Metal::DeviceContext& context)
    {
        context.Bind(Metal::Topology::TriangleStrip);
        context.Unbind<Metal::VertexBuffer>();
        context.Unbind<Metal::BoundInputLayout>();
    }

    void BuildGaussianFilteringWeights(float result[], float standardDeviation, unsigned weightsCount)
    {
            //      Interesting experiment with gaussian blur standard deviation values here:
            //          http://theinstructionlimit.com/tag/gaussian-blur
        float total = 0.f;
        for (int c=0; c<int(weightsCount); ++c) {
            const int centre = weightsCount / 2;
            unsigned xb = XlAbs(centre - c);
            result[c] = std::exp(-float(xb * xb) / (2.f * standardDeviation * standardDeviation));
            total += result[c];
        }
            //  have to balance the weights so they add up to one -- otherwise
            //  the final result will be too bright/dark
        for (unsigned c=0; c<weightsCount; ++c) {
            result[c] /= total;
        }
    }

    float PowerForHalfRadius(float halfRadius, float powerFraction)
    {
        const float attenuationScalar = 1.f;
        return (attenuationScalar*(halfRadius*halfRadius)+1.f) * (1.0f / (1.f-powerFraction));
    }

    ResourcePtr CreateResourceImmediate(const BufferUploads::BufferDesc& desc)
    {
        return GetBufferUploads().Transaction_Immediate(desc)->AdoptUnderlying();
    }


    template<int Count>
        class UCS4Buffer
        {
        public:
            ucs4 _buffer[Count];
            UCS4Buffer(const char input[])
            {
                utf8_2_ucs4((const utf8*)input, XlStringLen(input), _buffer, dimof(_buffer));
            }
            UCS4Buffer(const std::string& input)
            {
                utf8_2_ucs4((const utf8*)input.c_str(), input.size(), _buffer, dimof(_buffer));
            }

            operator const ucs4*() const { return _buffer; }
        };

    void DrawPendingResources(   
        Metal::DeviceContext* context, 
        SceneEngine::LightingParserContext& parserContext, 
        RenderOverlays::Font* font)
    {
        if (    parserContext._pendingAssets.empty()
            &&  parserContext._invalidAssets.empty()
            &&  parserContext._errorString.empty())
            return;

        context->Bind(Techniques::CommonResources()._blendStraightAlpha);

        using namespace RenderOverlays;
        TextStyle   style(*font); 
        Float2 textPosition(8.f, 8.f);
        float lineHeight = font->LineHeight();
        const UiAlign alignment = UIALIGN_TOP_LEFT;
        const unsigned colour = 0xff7f7f7fu;

        if (!parserContext._pendingAssets.empty()) {
            UCS4Buffer<64> text("Pending assets:");
            Float2 alignedPosition = style.AlignText(Quad::MinMax(textPosition, Float2(1024.f, 1024.f)), alignment, text);
            style.Draw(
                context, alignedPosition[0], alignedPosition[1], text, -1,
                0.f, 1.f, 0.f, 0.f, 0xffff7f7f, UI_TEXT_STATE_NORMAL, true, nullptr);
            textPosition[1] += lineHeight;

            for (auto i=parserContext._pendingAssets.cbegin(); i!=parserContext._pendingAssets.cend(); ++i) {
                UCS4Buffer<256> text(*i);
                Float2 alignedPosition = style.AlignText(Quad::MinMax(textPosition + Float2(32.f, 0.f), Float2(1024.f, 1024.f)), alignment, text);
                style.Draw(
                    context, alignedPosition[0], alignedPosition[1], text, -1,
                    0.f, 1.f, 0.f, 0.f, colour, UI_TEXT_STATE_NORMAL, true, nullptr);
                textPosition[1] += lineHeight;
            }
        }

        if (!parserContext._invalidAssets.empty()) {
            UCS4Buffer<64> text("Invalid assets:");
            Float2 alignedPosition = style.AlignText(Quad::MinMax(textPosition, Float2(1024.f, 1024.f)), alignment, text);
            style.Draw(
                context, alignedPosition[0], alignedPosition[1], text, -1,
                0.f, 1.f, 0.f, 0.f, colour, UI_TEXT_STATE_NORMAL, true, nullptr);
            textPosition[1] += lineHeight;

            for (auto i=parserContext._invalidAssets.cbegin(); i!=parserContext._invalidAssets.cend(); ++i) {
                UCS4Buffer<256> text(*i);
                Float2 alignedPosition = style.AlignText(Quad::MinMax(textPosition + Float2(32.f, 0.f), Float2(1024.f, 1024.f)), alignment, text);
                style.Draw(
                    context, alignedPosition[0], alignedPosition[1], text, -1,
                    0.f, 1.f, 0.f, 0.f, colour, UI_TEXT_STATE_NORMAL, true, nullptr);
                textPosition[1] += lineHeight;
            }
        }

        if (!parserContext._errorString.empty()) {
            UCS4Buffer<512> text(parserContext._errorString);
            Float2 alignedPosition = style.AlignText(Quad::MinMax(textPosition, Float2(1024.f, 1024.f)), alignment, text);
            style.Draw(
                context, alignedPosition[0], alignedPosition[1], text, -1,
                0.f, 1.f, 0.f, 0.f, colour, UI_TEXT_STATE_NORMAL, true, nullptr);
            textPosition[1] += lineHeight;
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    IteratorRange<RenderCore::Assets::DelayStep*> AsDelaySteps(
        SceneParseSettings::BatchFilter filter)
    {
        using BF = SceneEngine::SceneParseSettings::BatchFilter;
        using V = std::vector<RenderCore::Assets::DelayStep>;
        using DelayStep = RenderCore::Assets::DelayStep;

        switch (filter) {
        case BF::General:
        case BF::PreDepth:
            {
                static DelayStep result[] { DelayStep::OpaqueRender };
                return MakeIteratorRange(result);
            }

        case BF::Transparent:
            {
                static DelayStep result[] { DelayStep::PostDeferred };
                return MakeIteratorRange(result);
            }
        
        case BF::TransparentPreDepth:
            {
                static DelayStep result[] { DelayStep::PostDeferred, DelayStep::SortedBlending };
                return MakeIteratorRange(result);
            }
        
        case BF::OITransparent:
            {
                static DelayStep result[] { DelayStep::SortedBlending };
                return MakeIteratorRange(result);
            }
        
        case BF::DMShadows:
        case BF::RayTracedShadows:
            {
                static DelayStep result[] { DelayStep::OpaqueRender, DelayStep::PostDeferred, DelayStep::SortedBlending };
                return MakeIteratorRange(result);
            }
        }

        return IteratorRange<RenderCore::Assets::DelayStep*>();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    ProtectState::ProtectState(Metal::DeviceContext& context, States::BitField states)
    : _context(&context), _states(states)
    {
        if (_states & States::RenderTargets || _states & States::Viewports)
            _targets = SavedTargets(context);
        if (_states & States::DepthStencilState)
            _depthStencilState = Metal::DepthStencilState(context);
        if (_states & States::BlendState) {
            ID3D::BlendState* rawptr = nullptr;
            context.GetUnderlying()->OMGetBlendState(&rawptr, _blendFactor, &_blendSampleMask);
            _blendState = moveptr(rawptr);
        }
        if (_states & States::InputLayout)
            _inputLayout = Metal::BoundInputLayout(context);

        if (_states & States::VertexBuffer) {
            ID3D::Buffer* rawptrs[s_vbCount];
            context.GetUnderlying()->IAGetVertexBuffers(0, s_vbCount, rawptrs, _vbStrides, _vbOffsets);
            for (unsigned c=0; c<s_vbCount; ++c)
                _vertexBuffers[c] = moveptr(rawptrs[c]);
        }

        if (_states & States::IndexBuffer) {
            ID3D::Buffer* rawptr = nullptr;
            context.GetUnderlying()->IAGetIndexBuffer(&rawptr, (DXGI_FORMAT*)&_ibFormat, &_ibOffset);
            _indexBuffer = moveptr(rawptr);
        }

        if (_states & States::Topology) {
            context.GetUnderlying()->IAGetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY*)&_topology);
        }
    }

    ProtectState::ProtectState()
    {
        _context = nullptr;
        _states = 0;
        _ibFormat = DXGI_FORMAT_UNKNOWN;
        _ibOffset = 0;
        _topology = (D3D11_PRIMITIVE_TOPOLOGY)0;
    }

    ProtectState::ProtectState(ProtectState&& moveFrom)
    : _targets(std::move(moveFrom._targets))
    , _depthStencilState(std::move(moveFrom._depthStencilState))
    , _inputLayout(std::move(moveFrom._inputLayout))
    , _indexBuffer(std::move(moveFrom._indexBuffer))
    , _blendState(std::move(moveFrom._blendState))
    {
        _context = moveFrom._context; moveFrom._context = nullptr;
        _states = moveFrom._states; moveFrom._states = 0;
        
        _ibFormat = moveFrom._ibFormat;
        _ibOffset = moveFrom._ibOffset;

        for (unsigned s=0; s<s_vbCount; ++s) {
            _vertexBuffers[s] = std::move(moveFrom._vertexBuffers[s]);
            _vbStrides[s] = moveFrom._vbStrides[s];
            _vbOffsets[s] = moveFrom._vbOffsets[s];
        }

        for (unsigned c=0; c<4; ++c)
            _blendFactor[c] = moveFrom._blendFactor[c];
        _blendSampleMask = moveFrom._blendSampleMask;

        _viewports = moveFrom._viewports;
        _topology = moveFrom._topology;
    }

    ProtectState& ProtectState::operator=(ProtectState&& moveFrom)
    {
        _context = moveFrom._context; moveFrom._context = nullptr;
        _targets = std::move(moveFrom._targets);
        _states = moveFrom._states; moveFrom._states = 0;

        _depthStencilState = std::move(moveFrom._depthStencilState);
        _inputLayout = std::move(moveFrom._inputLayout);

        _indexBuffer = std::move(moveFrom._indexBuffer);
        _ibFormat = moveFrom._ibFormat;
        _ibOffset = moveFrom._ibOffset;

        for (unsigned s=0; s<s_vbCount; ++s) {
            _vertexBuffers[s] = std::move(moveFrom._vertexBuffers[s]);
            _vbStrides[s] = moveFrom._vbStrides[s];
            _vbOffsets[s] = moveFrom._vbOffsets[s];
        }

        _blendState = std::move(moveFrom._blendState);
        for (unsigned c=0; c<4; ++c)
            _blendFactor[c] = moveFrom._blendFactor[c];
        _blendSampleMask = moveFrom._blendSampleMask;

        _viewports = moveFrom._viewports;
        _topology = moveFrom._topology;
        return *this;
    }

    void ProtectState::ResetStates()
    {
        if (_context) {
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
                _context->GetUnderlying()->IASetIndexBuffer(_indexBuffer.get(), (DXGI_FORMAT)_ibFormat, _ibOffset);
            if (_states & States::Topology)
                _context->GetUnderlying()->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)_topology);

            _states = 0;
        }
    }

    ProtectState::~ProtectState()
    {
        ResetStates();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void ShaderBasedCopy(
        Metal::DeviceContext& context,
        const Metal::DepthStencilView& dest,
        const Metal::ShaderResourceView& src,
        ProtectState::States::BitField protectStates)
    {
        using States = ProtectState::States;
        const States::BitField effectedStates = 
            States::RenderTargets | States::Viewports | States::DepthStencilState 
            | States::Topology | States::InputLayout | States::VertexBuffer
            ;
        ProtectState savedStates(context, effectedStates & protectStates);

        Metal::TextureDesc2D desc(dest.GetUnderlying());
        context.Bind(Metal::ViewportDesc(0.f, 0.f, float(desc.Width), float(desc.Height)));

        context.Bind(ResourceList<Metal::RenderTargetView, 0>(), &dest);
        context.Bind(Techniques::CommonResources()._dssWriteOnly);
        context.Bind(
            ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2d.vsh:fullscreen:vs_*",
                "game/xleres/basic.psh:copy_depth:ps_*"));
        context.BindPS(MakeResourceList(src));
        SetupVertexGeneratorShader(context);
        context.Draw(4);
        context.UnbindPS<Metal::ShaderResourceView>(0, 1);
    }

    class ShaderBasedCopyRes
    {
    public:
        class Desc {};

        const Metal::ShaderProgram* _shader;
        Metal::BoundUniforms _uniforms;

        ShaderBasedCopyRes(const Desc&)
        {
            _shader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2d.vsh:screenspacerect:vs_*",
                "game/xleres/basic.psh:copy_bilinear:ps_*");
            _uniforms = Metal::BoundUniforms(*_shader);
            _uniforms.BindConstantBuffers(1, {"ScreenSpaceOutput"});

            _validationCallback = _shader->GetDependencyValidation();
        }

        ~ShaderBasedCopyRes() {}

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const     
            { return _validationCallback; }

    private:
        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;
    };

    void ShaderBasedCopy(
        Metal::DeviceContext& context,
        const RenderCore::Metal::RenderTargetView& dest,
        const RenderCore::Metal::ShaderResourceView& src,
        std::pair<UInt2, UInt2> destination,
        std::pair<UInt2, UInt2> source,
        ProtectState::States::BitField protectStates)
    {
        using States = ProtectState::States;
        const States::BitField effectedStates = 
            States::RenderTargets | States::Viewports | States::DepthStencilState 
            | States::Topology | States::InputLayout | States::VertexBuffer
            ;
        ProtectState savedStates(context, effectedStates & protectStates);

        auto& res = Techniques::FindCachedBox2<ShaderBasedCopyRes>();

        Metal::TextureDesc2D desc(dest.GetUnderlying());
        context.Bind(Metal::ViewportDesc(0.f, 0.f, float(desc.Width), float(desc.Height)));

        Metal::TextureDesc2D srcDesc(src.GetUnderlying());

        Float2 coords[4] = 
        {
            Float2(destination.first[0] / float(desc.Width), destination.first[1] / float(desc.Height)), 
            Float2(destination.second[0] / float(desc.Width), destination.second[1] / float(desc.Height)),
            Float2(source.first[0] / float(srcDesc.Width), source.first[1] / float(srcDesc.Height)), 
            Float2(source.second[0] / float(srcDesc.Width), source.second[1] / float(srcDesc.Height))
        };

        context.Bind(MakeResourceList(dest), nullptr);
        context.Bind(Techniques::CommonResources()._dssWriteOnly);
        context.Bind(*res._shader);
        res._uniforms.Apply(context, Metal::UniformsStream(), 
            Metal::UniformsStream({MakeSharedPkt(coords)}, {}));
        context.BindPS(MakeResourceList(src));
        SetupVertexGeneratorShader(context);
        context.Draw(4);
        context.UnbindPS<Metal::ShaderResourceView>(0, 1);
    }

}

