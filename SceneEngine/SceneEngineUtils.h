// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once 

#include "SceneParser.h"    // for SceneParseSettings
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/Format.h"
#include "../BufferUploads/IBufferUploads_Forward.h"

#include "../RenderCore/DX11/Metal/DX11.h"

namespace RenderOverlays { class Font; }
namespace BufferUploads { struct BufferDesc; struct TextureDesc; namespace BindFlag { typedef unsigned BitField; }}
namespace RenderCore { namespace Assets { enum class DelayStep : unsigned; }}

namespace Utility
{
    template<typename Type> class IteratorRange;
}

namespace SceneEngine
{
        // todo -- avoid D3D11 specific types here
    class SavedTargets
    {
    public:
        SavedTargets(RenderCore::Metal::DeviceContext& context);
        ~SavedTargets();

        void        ResetToOldTargets(RenderCore::Metal::DeviceContext& context);
        ID3D::DepthStencilView*     GetDepthStencilView() { return _oldDepthTarget; }
        ID3D::RenderTargetView**    GetRenderTargets() { return _oldTargets; }
        const RenderCore::Metal::ViewportDesc*       GetViewports() { return _oldViewports; }
        
        void SetDepthStencilView(ID3D::DepthStencilView* dsv);

        class ResetMarker
        {
        public:
            ~ResetMarker();
            ResetMarker(ResetMarker&&);
            ResetMarker& operator=(ResetMarker&&);

        private:
            ResetMarker(SavedTargets& targets, RenderCore::Metal::DeviceContext& context);
            ResetMarker(const ResetMarker&) = delete;
            ResetMarker& operator=(const ResetMarker&) = delete;
            SavedTargets* _targets;
            RenderCore::Metal::DeviceContext* _context;
            friend class SavedTargets;
        };

        ResetMarker MakeResetMarker(RenderCore::Metal::DeviceContext& context) { return ResetMarker(*this, context); }

        static const unsigned MaxSimultaneousRenderTargetCount = 8; // D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT
        static const unsigned MaxViewportAndScissorRectCount = 16;  // D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE
    private:
        ID3D::RenderTargetView* _oldTargets[MaxSimultaneousRenderTargetCount];
        ID3D::DepthStencilView* _oldDepthTarget;
        RenderCore::Metal::ViewportDesc _oldViewports[MaxViewportAndScissorRectCount];
        unsigned _oldViewportCount;
    };

    class SavedBlendAndRasterizerState
    {
    public:
        void ResetToOldStates(RenderCore::Metal::DeviceContext& context);
        SavedBlendAndRasterizerState(RenderCore::Metal::DeviceContext& context);
        ~SavedBlendAndRasterizerState();
    protected:
        intrusive_ptr<ID3D::RasterizerState> _oldRasterizerState;
        intrusive_ptr<ID3D::BlendState> _oldBlendState;
        float _oldBlendFactor[4]; unsigned _oldSampleMask;
    };

    BufferUploads::IManager& GetBufferUploads();

    BufferUploads::BufferDesc BuildRenderTargetDesc( 
        BufferUploads::BindFlag::BitField bindFlags, 
        const BufferUploads::TextureDesc& textureDesc,
        const char name[]);

    void SetupVertexGeneratorShader(RenderCore::Metal::DeviceContext& context);
    void BuildGaussianFilteringWeights(float result[], float standardDeviation, unsigned weightsCount);
    float PowerForHalfRadius(float halfRadius, float powerFraction=0.5f);

    class LightingParserContext;
    void DrawPendingResources(
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext, 
        RenderOverlays::Font* font);

    class FormatStack
    {
    public:
        typedef RenderCore::Metal::NativeFormat::Enum Format;
        Format _resourceFormat;
        Format _shaderReadFormat;
        Format _writeFormat;

        FormatStack() : _resourceFormat(RenderCore::Metal::NativeFormat::Unknown), _shaderReadFormat(RenderCore::Metal::NativeFormat::Unknown), _writeFormat(RenderCore::Metal::NativeFormat::Unknown) {}
        FormatStack(Format format) : _resourceFormat(format), _shaderReadFormat(format), _writeFormat(format) {}
        FormatStack(Format resourceFormat, Format shaderReadFormat, Format writeFormat) : _resourceFormat(resourceFormat), _shaderReadFormat(shaderReadFormat), _writeFormat(writeFormat) {}
    };

        // Note --  hard coded set of technique indices. This non-ideal in the sense that it limits
        //          the number of different ways we can render things. But it's also important for
        //          performance, since technique lookups can happen very frequently. It's hard to
        //          find a good balance between performance and flexibility for this case.
    static const unsigned TechniqueIndex_General   = 0;
    static const unsigned TechniqueIndex_DepthOnly = 1;
    static const unsigned TechniqueIndex_Deferred  = 2;
    static const unsigned TechniqueIndex_ShadowGen = 3;

    static const unsigned TechniqueIndex_OrderIndependentTransparency = 4;
    static const unsigned TechniqueIndex_RTShadowGen = 9;

    typedef intrusive_ptr<ID3D::Resource>      ResourcePtr;
    ResourcePtr         CreateResourceImmediate(const BufferUploads::BufferDesc& desc);

        //  Currently there is no flexible way to set material parameters
        //  there's just a single global set of material values...
    
    class MaterialOverride
    {
    public:
        float   _metallic;
        float   _roughness;
        float   _specular;
        float   _specular2;
        float   _material;
        float   _material2;
        float   _diffuseScale;
        float   _reflectionsScale;
        float   _reflectionsBoost;
        float   _specular0Scale;
        float   _specular1Scale;
        float   _dummy[1];
    };

    extern MaterialOverride GlobalMaterialOverride;

    inline Float3 AsFloat3Color(unsigned packedColor)
    {
        return Float3(
            (float)((packedColor >> 16) & 0xff) / 255.f,
            (float)((packedColor >>  8) & 0xff) / 255.f,
            (float)(packedColor & 0xff) / 255.f);
    }

    IteratorRange<RenderCore::Assets::DelayStep*> AsDelaySteps(
        SceneParseSettings::BatchFilter filter);

///////////////////////////////////////////////////////////////////////////////////////////////////

    inline SavedTargets::ResetMarker::~ResetMarker()
    {
        if (_targets && _context)
            _targets->ResetToOldTargets(*_context);
    }

    inline SavedTargets::ResetMarker::ResetMarker(ResetMarker&& moveFrom)
    {
        _targets = moveFrom._targets; moveFrom._targets = nullptr;
        _context = moveFrom._context; moveFrom._context = nullptr;
    }

    inline auto SavedTargets::ResetMarker::operator=(ResetMarker&& moveFrom) -> ResetMarker&
    {
        _targets = moveFrom._targets; moveFrom._targets = nullptr;
        _context = moveFrom._context; moveFrom._context = nullptr;
        return *this;
    }

    inline SavedTargets::ResetMarker::ResetMarker(SavedTargets& targets, RenderCore::Metal::DeviceContext& context)
    : _targets(&targets), _context(&context)
    {}
    
}

