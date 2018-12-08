// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "HighlightEffects.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/TextureView.h"
#include "../../RenderCore/Metal/Resource.h"
#include "../../RenderCore/Metal/ObjectFactory.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Metal/Resource.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/Techniques/RenderPassUtils.h"
#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Format.h"
#include "../../RenderCore/BufferView.h"
#include "../../Assets/Assets.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../Utility/StringFormat.h"

namespace RenderOverlays
{
    using namespace RenderCore;

	std::shared_ptr<Metal::ShaderProgram> LoadShaderProgram(
		StringSection<> vs,
		StringSection<> ps,
		StringSection<> definesTable = {})
	{
		auto vsCode = ::Assets::MakeAsset<CompiledShaderByteCode>(vs, definesTable);
		auto psCode = ::Assets::MakeAsset<CompiledShaderByteCode>(ps, definesTable);
		auto vsActual = vsCode->Actualize();
		auto psActual = psCode->Actualize();
		return std::make_shared<Metal::ShaderProgram>(Metal::GetObjectFactory(), *vsActual, *psActual);
	}

    const UInt4 HighlightByStencilSettings::NoHighlight = UInt4(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);

    HighlightByStencilSettings::HighlightByStencilSettings()
    {
        _outlineColor = Float3(1.5f, 1.35f, .7f);

        _highlightedMarker = NoHighlight;
        for (unsigned c=0; c<dimof(_stencilToMarkerMap); ++c) 
            _stencilToMarkerMap[c] = NoHighlight;
    }

    static void ExecuteHighlightByStencil(
        Metal::DeviceContext& metalContext,
        Metal::ShaderResourceView& stencilSrv,
        const HighlightByStencilSettings& settings,
        bool onlyHighlighted)
    {
		auto cbData = MakeIteratorRange(&settings, PtrAdd(&settings, sizeof(settings)));
        metalContext.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(Metal::MakeConstantBuffer(Metal::GetObjectFactory(), cbData)));
        metalContext.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(stencilSrv));
        metalContext.Bind(Techniques::CommonResources()._dssDisable);
        metalContext.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
        metalContext.Bind(Topology::TriangleStrip);
        metalContext.UnbindInputLayout();

        auto desc = stencilSrv.GetResource()->GetDesc();
        if (desc._type != ResourceDesc::Type::Texture) return;
        
        auto components = GetComponents(desc._textureDesc._format);
        bool stencilInput = 
                components == FormatComponents::DepthStencil
            ||  components == FormatComponents::Stencil;
                
        StringMeld<64, ::Assets::ResChar> params;
        params << "ONLY_HIGHLIGHTED=" << unsigned(onlyHighlighted);
        params << ";INPUT_MODE=" << (stencilInput?0:1);

        {
            auto shader = LoadShaderProgram(
                "xleres/basic2D.vsh:fullscreen:vs_*", 
                "xleres/Vis/HighlightVis.psh:HighlightByStencil:ps_*",
                (const ::Assets::ResChar*)params);
                
            metalContext.Bind(*shader);
            metalContext.Draw(4);
        }

        {
            auto shader = LoadShaderProgram(
                "xleres/basic2D.vsh:fullscreen:vs_*", 
                "xleres/Vis/HighlightVis.psh:OutlineByStencil:ps_*",
                (const ::Assets::ResChar*)params);
                
            metalContext.Bind(*shader);
            metalContext.Draw(4);
        }

        metalContext.GetNumericUniforms(ShaderStage::Pixel).Reset();
    }

    void ExecuteHighlightByStencil(
        RenderCore::IThreadContext& threadContext,
        RenderCore::Techniques::ParsingContext& parsingContext,
        const HighlightByStencilSettings& settings,
        bool onlyHighlighted)
    {
		auto rpi = RenderCore::Techniques::RenderPassToPresentationTarget(threadContext, parsingContext);

		Metal::ShaderResourceView* stencilSrv = nullptr;
		assert(0);
        /*auto stencilSrv = namedRes.GetSRV(
            RenderCore::Techniques::Attachments::MainDepthStencil,
            TextureViewDesc{
                {TextureViewDesc::Aspect::Stencil},
                TextureViewDesc::All, TextureViewDesc::All, TextureDesc::Dimensionality::Undefined,
				TextureViewDesc::Flags::JustStencil});
        if (!stencilSrv->IsGood()) return;*/

        auto metalContext = RenderCore::Metal::DeviceContext::Get(threadContext);
        ExecuteHighlightByStencil(*metalContext, *stencilSrv, settings, onlyHighlighted);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class HighlightShaders
    {
    public:
        class Desc {};

        std::shared_ptr<Metal::ShaderProgram> _drawHighlight;
        Metal::BoundUniforms _drawHighlightUniforms;

		std::shared_ptr<Metal::ShaderProgram> _drawShadow;
        Metal::BoundUniforms _drawShadowUniforms;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _validationCallback; }

        HighlightShaders(const Desc&);
    protected:
        std::shared_ptr<::Assets::DependencyValidation>  _validationCallback;
    };

    HighlightShaders::HighlightShaders(const Desc&)
    {
        //// ////
        _drawHighlight = LoadShaderProgram(
            "xleres/basic2D.vsh:fullscreen:vs_*", 
            "xleres/effects/outlinehighlight.psh:main:ps_*");
		UniformsStreamInterface drawHighlightInterface;
		drawHighlightInterface.BindConstantBuffer(0, { Hash64("$Globals") });
		_drawHighlightUniforms = Metal::BoundUniforms(*_drawHighlight, {}, {}, drawHighlightInterface);

        //// ////
        _drawShadow = LoadShaderProgram(
            "xleres/basic2D.vsh:fullscreen:vs_*", 
            "xleres/effects/outlinehighlight.psh:main_shadow:ps_*");
		UniformsStreamInterface drawShadowInterface;
		drawShadowInterface.BindConstantBuffer(0, { Hash64("ShadowHighlightSettings") });
		_drawShadowUniforms = Metal::BoundUniforms(*_drawShadow, {}, {}, drawShadowInterface);

        //// ////
        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_validationCallback, _drawHighlight->GetDependencyValidation());
    }

    class BinaryHighlight::Pimpl
    {
    public:
        IThreadContext*                 _threadContext;
        Techniques::AttachmentPool*	_namedRes;
        Techniques::RenderPassInstance  _rpi;

        Pimpl(IThreadContext& threadContext, Techniques::AttachmentPool& namedRes)
        : _threadContext(&threadContext), _namedRes(&namedRes) {}
        ~Pimpl() {}
    };

    BinaryHighlight::BinaryHighlight(
        IThreadContext& threadContext, 
		Techniques::FrameBufferPool& fbPool,
        Techniques::AttachmentPool& namedRes)
    {
        using namespace RenderCore;
        _pimpl = std::make_unique<Pimpl>(threadContext, namedRes);

		const bool doDepthTest = true;

		Techniques::FrameBufferDescFragment fbDescFrag;
		auto n_offscreen = fbDescFrag.DefineTemporaryAttachment(
			AttachmentDesc {
				Format::R8G8B8A8_UNORM, 1.f, 1.f, 0u,
				TextureViewDesc::ColorLinear,
				AttachmentDesc::DimensionsMode::OutputRelative, 
				AttachmentDesc::Flags::RenderTarget | AttachmentDesc::Flags::ShaderResource });
		auto n_mainColor = fbDescFrag.DefineAttachment(
			RenderCore::Techniques::AttachmentSemantics::ColorLDR,
			AsAttachmentDesc(namedRes.GetBoundResource(RenderCore::Techniques::AttachmentSemantics::ColorLDR)->GetDesc()));
		AttachmentName n_depth = ~0u;
		if (doDepthTest)
			n_depth = fbDescFrag.DefineAttachment(
				RenderCore::Techniques::AttachmentSemantics::MultisampleDepth,
				AsAttachmentDesc(namedRes.GetBoundResource(RenderCore::Techniques::AttachmentSemantics::MultisampleDepth)->GetDesc()));

		SubpassDesc subpass0;
		subpass0.AppendOutput(n_offscreen, LoadStore::Clear, LoadStore::Retain);
		subpass0.SetDepthStencil(n_depth);
		fbDescFrag.AddSubpass(std::move(subpass0));

		SubpassDesc subpass1;
		subpass1.AppendOutput(n_mainColor, LoadStore::Retain, LoadStore::Retain);
		subpass1.AppendInput(n_offscreen, LoadStore::Retain, LoadStore::DontCare);
		fbDescFrag.AddSubpass(std::move(subpass1));
        
		ClearValue clearValues[] = {MakeClearValue(0.f, 0.f, 0.f, 0.f)};
        _pimpl->_rpi = Techniques::RenderPassInstance(
            threadContext, Techniques::BuildFrameBufferDesc(std::move(fbDescFrag)), 
			fbPool, namedRes,
			{MakeIteratorRange(clearValues)});
    }

    void BinaryHighlight::FinishWithOutlineAndOverlay(RenderCore::IThreadContext& threadContext, Float3 outlineColor, unsigned overlayColor)
    {
        auto& srv = *_pimpl->_rpi.GetSRV(0);
        assert(srv.IsGood());
        if (!srv.IsGood()) return;

        _pimpl->_rpi.NextSubpass();

        static Float3 highlightColO(1.5f, 1.35f, .7f);
        static unsigned overlayColO = 1;

        outlineColor = highlightColO;
        overlayColor = overlayColO;

        auto metalContext = Metal::DeviceContext::Get(threadContext);

        HighlightByStencilSettings settings;
        settings._outlineColor = outlineColor;
        for (unsigned c=1; c<dimof(settings._stencilToMarkerMap); ++c)
            settings._stencilToMarkerMap[c] = UInt4(overlayColor, overlayColor, overlayColor, overlayColor);
        ExecuteHighlightByStencil(
            *metalContext, srv, 
            settings, false);

        _pimpl->_rpi.End();
    }

    void BinaryHighlight::FinishWithOutline(RenderCore::IThreadContext& threadContext, Float3 outlineColor)
    {
            //  now we can render these objects over the main image, 
            //  using some filtering

        auto& srv = *_pimpl->_rpi.GetSRV(0);
        assert(srv.IsGood());
        if (!srv.IsGood()) return;

        _pimpl->_rpi.NextSubpass();
        auto& metalContext = *Metal::DeviceContext::Get(threadContext);
        metalContext.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(srv));

        struct Constants { Float3 _color; unsigned _dummy; } constants = { outlineColor, 0 };
		ConstantBufferView cbvs[] = { MakeSharedPkt(constants) };

        auto& shaders = ConsoleRig::FindCachedBoxDep<HighlightShaders>(HighlightShaders::Desc());
        shaders._drawHighlightUniforms.Apply(metalContext, 1, { MakeIteratorRange(cbvs) });
        metalContext.Bind(*shaders._drawHighlight);
        metalContext.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
        metalContext.Bind(Techniques::CommonResources()._dssDisable);
        metalContext.Bind(Topology::TriangleStrip);
        metalContext.Draw(4);

        _pimpl->_rpi.End();
    }

    void BinaryHighlight::FinishWithShadow(RenderCore::IThreadContext& threadContext, Float4 shadowColor)
    {
        auto& srv = *_pimpl->_rpi.GetSRV(0);
        assert(srv.IsGood());
        if (srv.IsGood()) return;

            //  now we can render these objects over the main image, 
            //  using some filtering

        _pimpl->_rpi.NextSubpass();
        auto& metalContext = *Metal::DeviceContext::Get(threadContext);
        metalContext.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(srv));

        struct Constants { Float4 _shadowColor; } constants = { shadowColor };
		ConstantBufferView cbvs[] = { MakeSharedPkt(constants) };

        auto& shaders = ConsoleRig::FindCachedBoxDep<HighlightShaders>(HighlightShaders::Desc());
        shaders._drawShadowUniforms.Apply(metalContext, 1, { MakeIteratorRange(cbvs) });
        metalContext.Bind(*shaders._drawShadow);
        metalContext.Bind(Techniques::CommonResources()._blendStraightAlpha);
        metalContext.Bind(Techniques::CommonResources()._dssDisable);
        metalContext.Bind(Topology::TriangleStrip);
        metalContext.Draw(4);

        _pimpl->_rpi.End();
    }

    BinaryHighlight::~BinaryHighlight() {}

}

