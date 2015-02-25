// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SceneEngineUtility.h"

#include "LightingParserContext.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderOverlays/Font.h"

#include "../RenderCore/DX11/Metal/IncludeDX11.h"

namespace SceneEngine
{
    BufferUploads::IManager* g_ActiveBufferUploads = nullptr;
    BufferUploads::IManager* GetBufferUploads()
    {
        return g_ActiveBufferUploads;
    }

    void SetBufferUploads(BufferUploads::IManager* bufferUploads)
    {
        g_ActiveBufferUploads = bufferUploads;
    }

    SavedTargets::SavedTargets(RenderCore::Metal::DeviceContext* context)
    {
        _oldViewportCount = dimof(_oldViewports);
        std::fill(_oldTargets, &_oldTargets[dimof(_oldTargets)], nullptr);
        context->GetUnderlying()->OMGetRenderTargets(dimof(_oldTargets), _oldTargets, &_oldDepthTarget);
        context->GetUnderlying()->RSGetViewports(&_oldViewportCount, (D3D11_VIEWPORT*)_oldViewports);
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

    void        SavedTargets::ResetToOldTargets(RenderCore::Metal::DeviceContext* context)
    {
        context->GetUnderlying()->OMSetRenderTargets(dimof(_oldTargets), _oldTargets, _oldDepthTarget);
        context->GetUnderlying()->RSSetViewports(_oldViewportCount, (D3D11_VIEWPORT*)_oldViewports);
    }

    void SavedBlendAndRasterizerState::ResetToOldStates(RenderCore::Metal::DeviceContext* context)
    {
        context->GetUnderlying()->RSSetState(_oldRasterizerState.get());
        context->GetUnderlying()->OMSetBlendState(_oldBlendState.get(), _oldBlendFactor, _oldSampleMask);
    }

    SavedBlendAndRasterizerState::SavedBlendAndRasterizerState(RenderCore::Metal::DeviceContext* context)
    {
        ID3D::RasterizerState* rs = nullptr;
        ID3D::BlendState* bs = nullptr;
        context->GetUnderlying()->RSGetState(&rs);
        context->GetUnderlying()->OMGetBlendState(&bs, _oldBlendFactor, &_oldSampleMask);

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

    void SetupVertexGeneratorShader(RenderCore::Metal::DeviceContext* context)
    {
        using namespace RenderCore::Metal;
        context->Bind(Topology::TriangleStrip);
        context->Unbind<VertexBuffer>();
        context->Unbind<BoundInputLayout>();
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
        return GetBufferUploads()->Transaction_Immediate(desc, nullptr)->AdoptUnderlying();
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
        RenderCore::Metal::DeviceContext* context, 
        SceneEngine::LightingParserContext& parserContext, 
        RenderOverlays::Font* font)
    {
        if (    parserContext._pendingResources.empty()
            &&  parserContext._invalidResources.empty()
            &&  parserContext._errorString.empty())
            return;

        context->Bind(RenderCore::Techniques::CommonResources()._blendStraightAlpha);

        using namespace RenderOverlays;
        TextStyle   style(*font); 
        Float2 textPosition(8.f, 8.f);
        float lineHeight = font->LineHeight();
        const UiAlign alignment = UIALIGN_TOP_LEFT;
        const unsigned colour = 0xff7f7f7fu;

        if (!parserContext._pendingResources.empty()) {
            UCS4Buffer<64> text("Pending resources:");
            Float2 alignedPosition = style.AlignText(Quad::MinMax(textPosition, Float2(1024.f, 1024.f)), alignment, text);
            style.Draw(
                context, alignedPosition[0], alignedPosition[1], text, -1,
                0.f, 1.f, 0.f, 0.f, 0xffff7f7f, UI_TEXT_STATE_NORMAL, true, nullptr);
            textPosition[1] += lineHeight;

            for (auto i=parserContext._pendingResources.cbegin(); i!=parserContext._pendingResources.cend(); ++i) {
                UCS4Buffer<256> text(*i);
                Float2 alignedPosition = style.AlignText(Quad::MinMax(textPosition + Float2(32.f, 0.f), Float2(1024.f, 1024.f)), alignment, text);
                style.Draw(
                    context, alignedPosition[0], alignedPosition[1], text, -1,
                    0.f, 1.f, 0.f, 0.f, colour, UI_TEXT_STATE_NORMAL, true, nullptr);
                textPosition[1] += lineHeight;
            }
        }

        if (!parserContext._invalidResources.empty()) {
            UCS4Buffer<64> text("Invalid resources:");
            Float2 alignedPosition = style.AlignText(Quad::MinMax(textPosition, Float2(1024.f, 1024.f)), alignment, text);
            style.Draw(
                context, alignedPosition[0], alignedPosition[1], text, -1,
                0.f, 1.f, 0.f, 0.f, colour, UI_TEXT_STATE_NORMAL, true, nullptr);
            textPosition[1] += lineHeight;

            for (auto i=parserContext._invalidResources.cbegin(); i!=parserContext._invalidResources.cend(); ++i) {
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
}

