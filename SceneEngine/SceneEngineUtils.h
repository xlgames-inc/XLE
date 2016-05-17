// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once 

#include "SceneParser.h"    // for SceneParseSettings
#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Techniques/CommonBindings.h"
#include "../BufferUploads/IBufferUploads_Forward.h"
#include "../RenderCore/Assets/DelayedDrawCall.h"   // for DelayStep -- a forward declaration here confuses c++/cli

#if GFXAPI_ACTIVE == GFXAPI_DX11
	#include "../RenderCore/DX11/Metal/DX11.h"
#else
	#include "../RenderCore/Metal/TextureView.h"
#endif

namespace RenderOverlays { class Font; }
namespace RenderCore { class ResourceDesc; class TextureDesc; namespace BindFlag { typedef unsigned BitField; }; enum class Format; enum class UnderlyingAPI; }
namespace BufferUploads { class ResourceLocator; }

namespace Utility
{
    template<typename Type> class IteratorRange;
}

namespace SceneEngine
{
        // todo -- avoid D3D11 specific types here
#if GFXAPI_ACTIVE == GFXAPI_DX11
    class SavedTargets
    {
    public:
        SavedTargets(RenderCore::Metal::DeviceContext& context);
        SavedTargets();
        SavedTargets(SavedTargets&& moveFrom) never_throws;
        SavedTargets& operator=(SavedTargets&& moveFrom) never_throws;
        ~SavedTargets();

        void        ResetToOldTargets(RenderCore::Metal::DeviceContext& context);
        ID3D::DepthStencilView*     GetDepthStencilView() { return _oldDepthTarget; }
        ID3D::RenderTargetView**    GetRenderTargets() { return _oldTargets; }
        const RenderCore::Metal::ViewportDesc*       GetViewports() { return _oldViewports; }
        
        void SetDepthStencilView(ID3D::DepthStencilView* dsv);

        class ResetMarker
        {
        public:
            ResetMarker();
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
#else
	class SavedTargets
	{
	public:
		SavedTargets(RenderCore::Metal::DeviceContext& context);
		SavedTargets();
		SavedTargets(SavedTargets&& moveFrom) never_throws;
		SavedTargets& operator=(SavedTargets&& moveFrom) never_throws;
		~SavedTargets();

		void        ResetToOldTargets(RenderCore::Metal::DeviceContext& context);
		const RenderCore::Metal::DepthStencilView&		GetDepthStencilView() { return _oldDepthTarget; }
		RenderCore::Metal::RenderTargetView*			GetRenderTargets() { return _oldTargets; }
		const RenderCore::Metal::ViewportDesc*       GetViewports() { return _oldViewports; }

		void SetDepthStencilView(const RenderCore::Metal::DepthStencilView& dsv);

		class ResetMarker
		{
		public:
			ResetMarker();
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
		RenderCore::Metal::RenderTargetView _oldTargets[MaxSimultaneousRenderTargetCount];
		RenderCore::Metal::DepthStencilView _oldDepthTarget;
		RenderCore::Metal::ViewportDesc _oldViewports[MaxViewportAndScissorRectCount];
		unsigned _oldViewportCount;
	};
#endif

    BufferUploads::IManager& GetBufferUploads();

    RenderCore::ResourceDesc BuildRenderTargetDesc( 
		RenderCore::BindFlag::BitField bindFlags,
        const RenderCore::TextureDesc& textureDesc,
        const char name[]);

    Int2 GetCursorPos();
    bool IsLButtonDown();
    bool IsShiftDown();

    void SetupVertexGeneratorShader(RenderCore::Metal::DeviceContext& context);
    void BuildGaussianFilteringWeights(float result[], float standardDeviation, unsigned weightsCount);
    float PowerForHalfRadius(float halfRadius, float powerFraction=0.5f);

    void CheckSpecularIBLMipMapCount(const RenderCore::Metal::ShaderResourceView& srv);

    class LightingParserContext;
    void DrawPendingResources(
        RenderCore::IThreadContext& context, 
        LightingParserContext& parserContext, 
        RenderOverlays::Font* font);
    void DrawQuickMetrics(   
        RenderCore::IThreadContext& context, 
        SceneEngine::LightingParserContext& parserContext, 
        RenderOverlays::Font* font);

    class FormatStack
    {
    public:
        using Format = RenderCore::Format;
        Format _resourceFormat;
        Format _shaderReadFormat;
        Format _writeFormat;

        FormatStack() : _resourceFormat(Format(0)), _shaderReadFormat(Format(0)), _writeFormat(Format(0)) {}
        FormatStack(Format format) : _resourceFormat(format), _shaderReadFormat(format), _writeFormat(format) {}
        FormatStack(Format resourceFormat, Format shaderReadFormat, Format writeFormat) : _resourceFormat(resourceFormat), _shaderReadFormat(shaderReadFormat), _writeFormat(writeFormat) {}
    };

    static const auto TechniqueIndex_General   = RenderCore::Techniques::TechniqueIndex::Forward;
    static const auto TechniqueIndex_DepthOnly = RenderCore::Techniques::TechniqueIndex::DepthOnly;
    static const auto TechniqueIndex_Deferred  = RenderCore::Techniques::TechniqueIndex::Deferred;
    static const auto TechniqueIndex_ShadowGen = RenderCore::Techniques::TechniqueIndex::ShadowGen;

    static const auto TechniqueIndex_OrderIndependentTransparency = RenderCore::Techniques::TechniqueIndex::OrderIndependentTransparency;
    static const auto TechniqueIndex_RTShadowGen = RenderCore::Techniques::TechniqueIndex::WriteTriangleIndex;
    static const auto TechniqueIndex_StochasticTransparency = RenderCore::Techniques::TechniqueIndex::StochasticTransparency;
    static const auto TechniqueIndex_DepthWeightedTransparency = RenderCore::Techniques::TechniqueIndex::DepthWeightedTransparency;

    typedef intrusive_ptr<BufferUploads::ResourceLocator>      ResourcePtr;
    ResourcePtr         CreateResourceImmediate(const RenderCore::ResourceDesc& desc);

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

    inline unsigned AsPackedColor(Float3 col)
    {
        return 
            (unsigned(Clamp(col[0], 0.f, 1.f) * 255.f) << 16u)
        |   (unsigned(Clamp(col[1], 0.f, 1.f) * 255.f) <<  8u)
        |   (unsigned(Clamp(col[2], 0.f, 1.f) * 255.f) <<  0u)
        |   0xff000000
        ;
    }

    IteratorRange<RenderCore::Assets::DelayStep*> AsDelaySteps(
        SceneParseSettings::BatchFilter filter);

///////////////////////////////////////////////////////////////////////////////////////////////////

    /// <summary>Low-level save and restore of state information</summary>
    /// Handy utility for functions that want to restore the low-level GFX API
    /// state back to it's original state. 
    ///
    /// Often rendering utility functions need to change certain states on a temporary
    /// basis. This can be confusing to the caller, because it's often not clear what
    /// states will be affected.
    ///
    /// States captured in the constructor will be restored to their previous values in the
    /// destructor.
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
                RasterizerState     = 1<<4,
                Topology            = 1<<5,
                InputLayout         = 1<<6,
                VertexBuffer        = 1<<7,
                IndexBuffer         = 1<<8
            };
            using BitField = unsigned;
        };

        ProtectState();
        ProtectState(RenderCore::Metal::DeviceContext& context, States::BitField states);
        ~ProtectState();
        ProtectState(ProtectState&& moveFrom);
        ProtectState& operator=(ProtectState&& moveFrom);

    private:
        RenderCore::Metal::DeviceContext* _context;
        SavedTargets        _targets;
        States::BitField    _states;
        
		#if GFXAPI_ACTIVE == GFXAPI_DX11
			RenderCore::Metal::DepthStencilState    _depthStencilState;
			RenderCore::Metal::BoundInputLayout     _inputLayout;

			intrusive_ptr<ID3D::Buffer> _indexBuffer;
			unsigned    _ibFormat; // DXGI_FORMAT
			unsigned    _ibOffset;

			static const auto s_vbCount = 32; // D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
			intrusive_ptr<ID3D::Buffer> _vertexBuffers[s_vbCount];
			unsigned    _vbStrides[s_vbCount];
			unsigned    _vbOffsets[s_vbCount];

			intrusive_ptr<ID3D::BlendState> _blendState;
			float       _blendFactor[4];
			unsigned    _blendSampleMask;

			RenderCore::Metal::RasterizerState _rasterizerState;

			RenderCore::Metal::ViewportDesc _viewports;

			unsigned _topology;     // D3D11_PRIMITIVE_TOPOLOGY
		#endif

        void ResetStates();
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    /// <summary>Copy from a shader resource onto a depth buffer, using a Draw operation<summary>
    void ShaderBasedCopy(
        RenderCore::Metal::DeviceContext& context,
        const RenderCore::Metal::DepthStencilView& dest,
        const RenderCore::Metal::ShaderResourceView& src,
        ProtectState::States::BitField protectStates = ~0u);

    enum class CopyFilter { Bilinear, BoxFilter, BoxFilterAlphaComplementWeight };

    void ShaderBasedCopy(
        RenderCore::Metal::DeviceContext& context,
        const RenderCore::Metal::RenderTargetView& dest,
        const RenderCore::Metal::ShaderResourceView& src,
        std::pair<UInt2, UInt2> destination,
        std::pair<UInt2, UInt2> source,
        CopyFilter filter = CopyFilter::Bilinear,
        ProtectState::States::BitField protectStates = ~0u);

///////////////////////////////////////////////////////////////////////////////////////////////////

    inline SavedTargets::ResetMarker::ResetMarker()
    {
        _targets = nullptr;
        _context = nullptr;
    }

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
        if (_targets && _context)
            _targets->ResetToOldTargets(*_context);

        _targets = moveFrom._targets; moveFrom._targets = nullptr;
        _context = moveFrom._context; moveFrom._context = nullptr;
        return *this;
    }

    inline SavedTargets::ResetMarker::ResetMarker(SavedTargets& targets, RenderCore::Metal::DeviceContext& context)
    : _targets(&targets), _context(&context)
    {}
    
}

