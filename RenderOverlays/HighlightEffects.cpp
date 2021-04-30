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
#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/Techniques/RenderPassUtils.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Format.h"
#include "../../RenderCore/BufferView.h"
#include "../../Assets/Assets.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../Utility/StringFormat.h"
#include "../../xleres/FileList.h"

namespace RenderOverlays
{
    using namespace RenderCore;

	::Assets::FuturePtr<Metal::ShaderProgram> LoadShaderProgram(
        const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout,
		StringSection<> vs,
		StringSection<> ps,
		StringSection<> definesTable = {})
	{
		return ::Assets::MakeAsset<Metal::ShaderProgram>(
            pipelineLayout,
            vs, ps, definesTable);
	}

    HighlightByStencilSettings::HighlightByStencilSettings()
    {
        _outlineColor = Float3(1.5f, 1.35f, .7f);
        _highlightedMarker = 0;
        _backgroundMarker = 0;
    }

    static void ExecuteHighlightByStencil(
        Metal::DeviceContext& metalContext,
        Metal::GraphicsEncoder_ProgressivePipeline& encoder,
        IResourceView* stencilSrv,
        const HighlightByStencilSettings& settings,
        bool onlyHighlighted)
    {
        assert(stencilSrv);
        UniformsStream::ImmediateData cbData[] = {
            MakeOpaqueIteratorRange(settings)
        };
        auto numericUniforms = encoder.BeginNumericUniformsInterface();
        numericUniforms.BindConstantBuffers(0, cbData);
        numericUniforms.Bind(0, MakeIteratorRange(&stencilSrv, &stencilSrv+1));
        numericUniforms.Apply(metalContext, encoder);
        encoder.Bind(Techniques::CommonResourceBox::s_dsDisable);
        encoder.Bind(MakeIteratorRange(&Techniques::CommonResourceBox::s_abAlphaPremultiplied, &Techniques::CommonResourceBox::s_abAlphaPremultiplied+1));
        encoder.Bind(Metal::BoundInputLayout{}, Topology::TriangleStrip);

        auto desc = stencilSrv->GetResource()->GetDesc();
        if (desc._type != ResourceDesc::Type::Texture) return;
        
        auto components = GetComponents(desc._textureDesc._format);
        bool stencilInput = 
                components == FormatComponents::DepthStencil
            ||  components == FormatComponents::Stencil;
                
        StringMeld<64, ::Assets::ResChar> params;
        params << "ONLY_HIGHLIGHTED=" << unsigned(onlyHighlighted);
        params << ";INPUT_MODE=" << (stencilInput?0:1);

        auto highlightShader = LoadShaderProgram(
            encoder.GetPipelineLayout(),
            BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
            HIGHLIGHT_VIS_PIXEL_HLSL ":HighlightByStencil:ps_*",
            params.AsStringSection())->TryActualize();
        if (highlightShader) {
            encoder.Bind(*highlightShader);
            encoder.Draw(4);
        }

        auto outlineShader = LoadShaderProgram(
            encoder.GetPipelineLayout(),
            BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
            HIGHLIGHT_VIS_PIXEL_HLSL ":OutlineByStencil:ps_*",
            params.AsStringSection())->TryActualize();
        if (outlineShader) {                
            encoder.Bind(*outlineShader);
            encoder.Draw(4);
        }
    }

    void ExecuteHighlightByStencil(
        IThreadContext& threadContext,
        Techniques::ParsingContext& parsingContext,
        std::shared_ptr<ICompiledPipelineLayout> pipelineLayout,
        const HighlightByStencilSettings& settings,
        bool onlyHighlighted)
    {
		std::vector<FrameBufferDesc::Attachment> attachments {
			{ Techniques::AttachmentSemantics::ColorLDR },
			{ Techniques::AttachmentSemantics::MultisampleDepth }
		};
		SubpassDesc mainPass;
		mainPass.SetName("VisualisationOverlay");
		mainPass.AppendOutput(0);
		mainPass.AppendInput(1);
		FrameBufferDesc fbDesc{ std::move(attachments), {mainPass}, parsingContext._fbProps };
		Techniques::RenderPassInstance rpi { threadContext, parsingContext, fbDesc };

        auto stencilSrv = rpi.GetInputAttachmentSRV(
            0,
            TextureViewDesc{
                {TextureViewDesc::Aspect::Stencil},
                TextureViewDesc::All, TextureViewDesc::All, TextureDesc::Dimensionality::Undefined,
				TextureViewDesc::Flags::JustStencil});
        if (!stencilSrv) return;

        auto& metalContext = *RenderCore::Metal::DeviceContext::Get(threadContext);
        auto encoder = metalContext.BeginGraphicsEncoder_ProgressivePipeline(pipelineLayout);
        ExecuteHighlightByStencil(metalContext, encoder, stencilSrv, settings, onlyHighlighted);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class HighlightShaders
    {
    public:
        std::shared_ptr<Metal::ShaderProgram> _drawHighlight;
        Metal::BoundUniforms _drawHighlightUniforms;

		std::shared_ptr<Metal::ShaderProgram> _drawShadow;
        Metal::BoundUniforms _drawShadowUniforms;

        const ::Assets::DependencyValidation& GetDependencyValidation() const { return _validationCallback; }

        HighlightShaders(std::shared_ptr<Metal::ShaderProgram> drawHighlight, std::shared_ptr<Metal::ShaderProgram> drawShadow);
        static void ConstructToFuture(
			::Assets::AssetFuture<HighlightShaders>&,
			const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout);
    protected:
        ::Assets::DependencyValidation  _validationCallback;
        
    };

    void HighlightShaders::ConstructToFuture(
        ::Assets::AssetFuture<HighlightShaders>& result,
        const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout)
    {
        //// ////
        auto drawHighlightFuture = LoadShaderProgram(
            pipelineLayout,
            BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
            OUTLINE_VIS_PIXEL_HLSL ":main:ps_*");
        auto drawShadowFuture = LoadShaderProgram(
            pipelineLayout,
            BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
            OUTLINE_VIS_PIXEL_HLSL ":main_shadow:ps_*");

        ::Assets::WhenAll(drawHighlightFuture, drawShadowFuture).ThenConstructToFuture(result);
    }

    HighlightShaders::HighlightShaders(std::shared_ptr<Metal::ShaderProgram> drawHighlight, std::shared_ptr<Metal::ShaderProgram> drawShadow)
    : _drawHighlight(std::move(drawHighlight))
    , _drawShadow(std::move(drawShadow))
    {
		UniformsStreamInterface drawHighlightInterface;
		drawHighlightInterface.BindImmediateData(0, Hash64("Settings"));
		_drawHighlightUniforms = Metal::BoundUniforms(*_drawHighlight, {}, {}, drawHighlightInterface);

        //// ////
        
		UniformsStreamInterface drawShadowInterface;
		drawShadowInterface.BindImmediateData(0, Hash64("ShadowHighlightSettings"));
		_drawShadowUniforms = Metal::BoundUniforms(*_drawShadow, {}, {}, drawShadowInterface);

        //// ////
        _validationCallback = ::Assets::GetDepValSys().Make();
        _validationCallback.RegisterDependency(_drawHighlight->GetDependencyValidation());
        _validationCallback.RegisterDependency(_drawShadow->GetDependencyValidation());
    }

    class BinaryHighlight::Pimpl
    {
    public:
        IThreadContext*                 _threadContext;
        std::shared_ptr<RenderCore::ICompiledPipelineLayout> _pipelineLayout;
        Techniques::AttachmentPool*	_namedRes;
        Techniques::RenderPassInstance  _rpi;
		FrameBufferDesc _fbDesc;

        Pimpl(IThreadContext& threadContext, std::shared_ptr<RenderCore::ICompiledPipelineLayout> pipelineLayout, Techniques::AttachmentPool& namedRes)
        : _threadContext(&threadContext), _pipelineLayout(std::move(pipelineLayout)), _namedRes(&namedRes) {}
        ~Pimpl() {}
    };

	const RenderCore::FrameBufferDesc& BinaryHighlight::GetFrameBufferDesc() const
	{
		return _pimpl->_fbDesc;
	}

    BinaryHighlight::BinaryHighlight(
        IThreadContext& threadContext, 
        std::shared_ptr<RenderCore::ICompiledPipelineLayout> pipelineLayout,
		Techniques::FrameBufferPool& fbPool,
        Techniques::AttachmentPool& namedRes,
        const FrameBufferProperties& fbProps)
    {
        using namespace RenderCore;
        _pimpl = std::make_unique<Pimpl>(threadContext, std::move(pipelineLayout), namedRes);

		Techniques::FrameBufferDescFragment fbDescFrag;
		auto n_offscreen = fbDescFrag.DefineTemporaryAttachment(
			AttachmentDesc {
				Format::R8G8B8A8_UNORM, 1.f, 1.f, 0u,
				AttachmentDesc::Flags::OutputRelativeDimensions });
		auto n_mainColor = fbDescFrag.DefineAttachment(RenderCore::Techniques::AttachmentSemantics::ColorLDR);
		const bool doDepthTest = true;
        auto n_depth = doDepthTest ? fbDescFrag.DefineAttachment(RenderCore::Techniques::AttachmentSemantics::MultisampleDepth) : ~0u;

		SubpassDesc subpass0;
		subpass0.AppendOutput(n_offscreen, LoadStore::Clear, LoadStore::Retain);
		subpass0.SetDepthStencil(AttachmentViewDesc { n_depth, LoadStore::Retain, LoadStore::Retain, TextureViewDesc{ TextureViewDesc::Aspect::DepthStencil } });
		fbDescFrag.AddSubpass(std::move(subpass0));

		SubpassDesc subpass1;
		subpass1.AppendOutput(n_mainColor, LoadStore::Retain, LoadStore::Retain);
		subpass1.AppendInput(n_offscreen, LoadStore::Retain, LoadStore::DontCare);
		fbDescFrag.AddSubpass(std::move(subpass1));
        
		ClearValue clearValues[] = {MakeClearValue(0.f, 0.f, 0.f, 0.f)};
		_pimpl->_fbDesc = Techniques::BuildFrameBufferDesc(std::move(fbDescFrag), fbProps);
        _pimpl->_rpi = Techniques::RenderPassInstance(
            threadContext, _pimpl->_fbDesc, 
			fbPool, namedRes,
            {},
			{MakeIteratorRange(clearValues)});
    }

    void BinaryHighlight::FinishWithOutlineAndOverlay(RenderCore::IThreadContext& threadContext, Float3 outlineColor, unsigned overlayColor)
    {
        auto* srv = _pimpl->_rpi.GetInputAttachmentSRV(0);
        assert(srv);

        _pimpl->_rpi.NextSubpass();

        if (srv) {
			static Float3 highlightColO(1.5f, 1.35f, .7f);
			static unsigned overlayColO = 1;

			outlineColor = highlightColO;
			overlayColor = overlayColO;

			auto& metalContext = *Metal::DeviceContext::Get(threadContext);

			HighlightByStencilSettings settings;
			settings._outlineColor = outlineColor;
            auto encoder = metalContext.BeginGraphicsEncoder_ProgressivePipeline(_pimpl->_pipelineLayout);
			ExecuteHighlightByStencil(
				metalContext, encoder, srv, 
				settings, false);
		}

        _pimpl->_rpi.End();
    }

    void BinaryHighlight::FinishWithOutline(RenderCore::IThreadContext& threadContext, Float3 outlineColor)
    {
            //  now we can render these objects over the main image, 
            //  using some filtering

        auto* srv = _pimpl->_rpi.GetInputAttachmentSRV(0);
        assert(srv);
        _pimpl->_rpi.NextSubpass();

        auto shaders = ::Assets::MakeAsset<HighlightShaders>(_pimpl->_pipelineLayout)->TryActualize();
		if (srv && shaders) {
			auto& metalContext = *Metal::DeviceContext::Get(threadContext);
            auto encoder = metalContext.BeginGraphicsEncoder_ProgressivePipeline(_pimpl->_pipelineLayout);
            auto numericUniforms = encoder.BeginNumericUniformsInterface();
			numericUniforms.Bind(MakeResourceList(*srv));        // (pixel)
            numericUniforms.Apply(metalContext, encoder);

			struct Constants { Float3 _color; unsigned _dummy; } constants = { outlineColor, 0 };

			shaders->_drawHighlightUniforms.ApplyLooseUniforms(metalContext, encoder, ImmediateDataStream{constants}, 1);
			encoder.Bind(*shaders->_drawHighlight);
			encoder.Bind(MakeIteratorRange(&Techniques::CommonResourceBox::s_abAlphaPremultiplied, &Techniques::CommonResourceBox::s_abAlphaPremultiplied+1));
			encoder.Bind(Techniques::CommonResourceBox::s_dsDisable);
			encoder.Bind({}, Topology::TriangleStrip);
			encoder.Draw(4);
		}

        _pimpl->_rpi.End();
    }

    void BinaryHighlight::FinishWithShadow(RenderCore::IThreadContext& threadContext, Float4 shadowColor)
    {
        auto* srv = _pimpl->_rpi.GetInputAttachmentSRV(0);
        assert(srv);
        _pimpl->_rpi.NextSubpass();

            //  now we can render these objects over the main image, 
            //  using some filtering

        auto shaders = ::Assets::MakeAsset<HighlightShaders>(_pimpl->_pipelineLayout)->TryActualize();
        if (srv && shaders) {
			auto& metalContext = *Metal::DeviceContext::Get(threadContext);
            auto encoder = metalContext.BeginGraphicsEncoder_ProgressivePipeline(_pimpl->_pipelineLayout);
            auto numericUniforms = encoder.BeginNumericUniformsInterface();
			numericUniforms.Bind(MakeResourceList(*srv));            // (pixel)
            numericUniforms.Apply(metalContext, encoder);

			struct Constants { Float4 _shadowColor; } constants = { shadowColor };

			shaders->_drawShadowUniforms.ApplyLooseUniforms(metalContext, encoder, ImmediateDataStream{constants}, 1);
			encoder.Bind(*shaders->_drawShadow);
			encoder.Bind(MakeIteratorRange(&Techniques::CommonResourceBox::s_abStraightAlpha, &Techniques::CommonResourceBox::s_abStraightAlpha+1));
			encoder.Bind(Techniques::CommonResourceBox::s_dsDisable);
			encoder.Bind({}, Topology::TriangleStrip);
			encoder.Draw(4);
		}

        _pimpl->_rpi.End();
    }

    BinaryHighlight::~BinaryHighlight() {}

}

