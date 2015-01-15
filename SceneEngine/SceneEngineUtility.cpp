// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SceneEngineUtility.h"

#include "LightingParserContext.h"
#include "CommonResources.h"
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

    BufferUploads::BufferDesc BuildRenderTargetDesc( BufferUploads::BindFlag::BitField bindFlags, 
                                                     const BufferUploads::TextureDesc& textureDesc)
    {
        using namespace BufferUploads;
        BufferDesc desc;
        desc._type = BufferDesc::Type::Texture;
        desc._bindFlags = bindFlags;
        desc._cpuAccess = 0;
        desc._gpuAccess = GPUAccess::Read|GPUAccess::Write;
        desc._allocationRules = 0;
        desc._textureDesc = textureDesc;
        return desc;
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



    void DrawPendingResources(   
        RenderCore::Metal::DeviceContext* context, 
        SceneEngine::LightingParserContext& parserContext, 
        RenderOverlays::Font* font)
    {
        if (    parserContext._pendingResources.empty()
            &&  parserContext._invalidResources.empty()
            &&  parserContext._errorString.empty())
            return;

        context->Bind(SceneEngine::CommonResources()._blendStraightAlpha);

        using namespace RenderOverlays;
        TextStyle   style(*font); 
        Float2 textPosition(8.f, 8.f);
        float lineHeight = font->LineHeight();

        if (!parserContext._pendingResources.empty()) {
            style.Draw(
                context, textPosition[0], textPosition[1], (ucs4*)L"Pending resources:", -1,
                0.f, 1.f, 0.f, 0.f, 0xffff7f7f, UI_TEXT_STATE_NORMAL, true, nullptr);
            textPosition[1] += lineHeight;

            for (auto i=parserContext._pendingResources.cbegin(); i!=parserContext._pendingResources.cend(); ++i) {
                ucs4     destination[64];
                XlMultiToWide(destination, dimof(destination), (const utf8*)i->c_str());
                style.Draw(
                    context, textPosition[0], textPosition[1], destination, -1,
                    0.f, 1.f, 0.f, 0.f, 0xffff7f7f, UI_TEXT_STATE_NORMAL, true, nullptr);
                textPosition[1] += lineHeight;
            }
        }

        if (!parserContext._invalidResources.empty()) {
            style.Draw(
                context, textPosition[0], textPosition[1], (ucs4*)L"Invalid resources:", -1,
                0.f, 1.f, 0.f, 0.f, 0xffff7f7f, UI_TEXT_STATE_NORMAL, true, nullptr);
            textPosition[1] += lineHeight;

            for (auto i=parserContext._invalidResources.cbegin(); i!=parserContext._invalidResources.cend(); ++i) {
                ucs4     destination[64];
                XlMultiToWide(destination, dimof(destination), (const utf8*)i->c_str());
                style.Draw(
                    context, textPosition[0] - 32, textPosition[1], destination, -1,
                    0.f, 1.f, 0.f, 0.f, 0xffff7f7f, UI_TEXT_STATE_NORMAL, true, nullptr);
                textPosition[1] += lineHeight;
            }
        }

        if (!parserContext._errorString.empty()) {
            ucs4     destination[256];
            XlMultiToWide(destination, dimof(destination), (const utf8*)parserContext._errorString.c_str());
            style.Draw(
                context, textPosition[0] - 32, textPosition[1], destination, -1,
                0.f, 1.f, 0.f, 0.f, 0xffff7f7f, UI_TEXT_STATE_NORMAL, true, nullptr);
            textPosition[1] += lineHeight;
        }
    }
}

