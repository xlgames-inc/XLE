// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ManipulatorsRender.h"
#include "../../SceneEngine/PlacementsManager.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/MetalStubs.h"
#include "../../SceneEngine/RenderStep.h"
#include "../../FixedFunctionModel/ModelRunTime.h"
#include "../../FixedFunctionModel/PreboundShaders.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/TextureView.h"
#include "../../RenderCore/Metal/ObjectFactory.h"
#include "../../RenderCore/Techniques/DeferredShaderResource.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/SimpleModelRenderer.h"
#include "../../RenderCore/Techniques/TechniqueDelegates.h"
#include "../../RenderCore/Techniques/PipelineAccelerator.h"
#include "../../RenderCore/Assets/PredefinedCBLayout.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/Format.h"
#include "../../RenderOverlays/HighlightEffects.h"
#include "../../Assets/Assets.h"
#include "../../Math/Transformations.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../xleres/FileList.h"

#include "../../RenderCore/DX11/Metal/DX11Utils.h"

namespace ToolsRig
{
    using namespace RenderCore;

    void Placements_RenderFiltered(
        RenderCore::IThreadContext& threadContext,
        Techniques::ParsingContext& parserContext,
        const RenderCore::Techniques::SequencerContext& sequencerTechnique,
        SceneEngine::PlacementsRenderer& renderer,
        const SceneEngine::PlacementCellSet& cellSet,
        const SceneEngine::PlacementGUID* filterBegin,
        const SceneEngine::PlacementGUID* filterEnd,
        uint64 materialGuid)
    {
		class PreDrawDelegate : public RenderCore::Techniques::IPreDrawDelegate
		{
		public:
			virtual bool OnDraw(
				Metal::DeviceContext&, RenderCore::Techniques::ParsingContext&,
				const RenderCore::Techniques::Drawable&,
				uint64_t materialGuid, unsigned drawCallIdx)
			{
				return materialGuid == _materialGuid;
			}
			uint64_t _materialGuid;
			PreDrawDelegate(uint64_t materialGuid) : _materialGuid(materialGuid) {}
		};

		using namespace RenderCore;
		using namespace SceneEngine;
		SceneExecuteContext sceneExeContext;
		auto viewDelegate = std::make_shared<BasicViewDelegate>();
		sceneExeContext.AddView({SceneView::Type::Normal, parserContext.GetProjectionDesc()}, viewDelegate);
		if (materialGuid == ~0ull) {
			renderer.BuildDrawables(
				sceneExeContext,
				cellSet, filterBegin, filterEnd);
		} else {
			auto del = std::make_shared<PreDrawDelegate>(materialGuid);
			renderer.BuildDrawables(sceneExeContext, cellSet, filterBegin, filterEnd, del);
		}

		Techniques::Draw(
			threadContext, parserContext, 
			sequencerTechnique, 
			viewDelegate->_pkt);
    }

	class TechniqueBox
	{
	public:
		std::shared_ptr<RenderCore::Techniques::TechniqueSetFile> _techniqueSetFile;
		std::shared_ptr<RenderCore::Techniques::TechniqueSharedResources> _techniqueSharedResources;
		std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate> _forwardIllumDelegate;

		const ::Assets::DependencyValidation& GetDependencyValidation() { return _techniqueSetFile->GetDependencyValidation(); }

		struct Desc {};
		TechniqueBox(const Desc&)
		{
			_techniqueSetFile = ::Assets::AutoConstructAsset<RenderCore::Techniques::TechniqueSetFile>(ILLUM_TECH);
			_techniqueSharedResources = std::make_shared<RenderCore::Techniques::TechniqueSharedResources>();
			_forwardIllumDelegate = RenderCore::Techniques::CreateTechniqueDelegate_Forward(_techniqueSetFile, _techniqueSharedResources, RenderCore::Techniques::TechniqueDelegateForwardFlags::DisableDepthWrite);
		}
	};

    void Placements_RenderHighlight(
        RenderCore::IThreadContext& threadContext,
        Techniques::ParsingContext& parserContext,
        SceneEngine::PlacementsRenderer& renderer,
        const SceneEngine::PlacementCellSet& cellSet,
        const SceneEngine::PlacementGUID* filterBegin,
        const SceneEngine::PlacementGUID* filterEnd,
        uint64 materialGuid)
    {
        CATCH_ASSETS_BEGIN
            RenderOverlays::BinaryHighlight highlight(threadContext, parserContext.GetFrameBufferPool(), parserContext.GetNamedResources());
			RenderCore::Techniques::SequencerContext seqContext;
			auto sequencerCfg = parserContext._pipelineAcceleratorPool->CreateSequencerConfig(
				ConsoleRig::FindCachedBoxDep2<TechniqueBox>()._forwardIllumDelegate, ParameterBox{}, 
				highlight.GetFrameBufferDesc());
			seqContext._sequencerConfig = sequencerCfg.get();
            Placements_RenderFiltered(
                threadContext, parserContext, 
				seqContext,
                renderer, cellSet, filterBegin, filterEnd, materialGuid);
            highlight.FinishWithOutline(threadContext, Float3(.65f, .8f, 1.5f));
        CATCH_ASSETS_END(parserContext)
    }

	void Placements_RenderHighlightWithOutlineAndOverlay(
        RenderCore::IThreadContext& threadContext,
        Techniques::ParsingContext& parserContext,
        SceneEngine::PlacementsRenderer& renderer,
        const SceneEngine::PlacementCellSet& cellSet,
		const SceneEngine::PlacementGUID* filterBegin,
        const SceneEngine::PlacementGUID* filterEnd,
        uint64 materialGuid)
    {
		CATCH_ASSETS_BEGIN
            RenderOverlays::BinaryHighlight highlight(threadContext, parserContext.GetFrameBufferPool(), parserContext.GetNamedResources());
			RenderCore::Techniques::SequencerContext seqContext;
			auto sequencerCfg = parserContext._pipelineAcceleratorPool->CreateSequencerConfig(
				ConsoleRig::FindCachedBoxDep2<TechniqueBox>()._forwardIllumDelegate, ParameterBox{}, 
				highlight.GetFrameBufferDesc());
			seqContext._sequencerConfig = sequencerCfg.get();
            Placements_RenderFiltered(
                threadContext, parserContext, 
				seqContext,
                renderer, cellSet, filterBegin, filterEnd, materialGuid);

			const Float3 highlightCol(.75f, .8f, 0.4f);
            const unsigned overlayCol = 2;

            highlight.FinishWithOutlineAndOverlay(threadContext, highlightCol, overlayCol);
        CATCH_ASSETS_END(parserContext)
	}

    void Placements_RenderShadow(
        RenderCore::IThreadContext& threadContext,
        Techniques::ParsingContext& parserContext,
        SceneEngine::PlacementsRenderer& renderer,
        const SceneEngine::PlacementCellSet& cellSet,
        const SceneEngine::PlacementGUID* filterBegin,
        const SceneEngine::PlacementGUID* filterEnd,
        uint64 materialGuid)
    {
        CATCH_ASSETS_BEGIN
            RenderOverlays::BinaryHighlight highlight(threadContext, parserContext.GetFrameBufferPool(), parserContext.GetNamedResources());
            RenderCore::Techniques::SequencerContext seqContext;
			auto sequencerCfg = parserContext._pipelineAcceleratorPool->CreateSequencerConfig(
				ConsoleRig::FindCachedBoxDep2<TechniqueBox>()._forwardIllumDelegate, ParameterBox{}, 
				highlight.GetFrameBufferDesc());
			seqContext._sequencerConfig = sequencerCfg.get();
			Placements_RenderFiltered(
                threadContext, parserContext, 
				seqContext,
                renderer, cellSet, filterBegin, filterEnd, materialGuid);
            highlight.FinishWithShadow(threadContext, Float4(.025f, .025f, .025f, 0.85f));
        CATCH_ASSETS_END(parserContext)
    }

    void RenderCylinderHighlight(
        IThreadContext& threadContext, 
        Techniques::ParsingContext& parserContext,
        const Float3& centre, float radius)
    {
		std::vector<FrameBufferDesc::Attachment> attachments {
			{ Techniques::AttachmentSemantics::ColorLDR, AsAttachmentDesc(parserContext.GetTechniqueContext()._attachmentPool->GetBoundResource(RenderCore::Techniques::AttachmentSemantics::ColorLDR)->GetDesc()) },
			{ Techniques::AttachmentSemantics::MultisampleDepth, Format::D24_UNORM_S8_UINT }
		};
		SubpassDesc mainPass;
		mainPass.SetName("RenderCylinderHighlight");
		mainPass.AppendOutput(0);
		mainPass.AppendInput(1);
		FrameBufferDesc fbDesc{ std::move(attachments), {mainPass} };
		Techniques::RenderPassInstance rpi {
			threadContext, fbDesc, 
			parserContext.GetFrameBufferPool(),
			parserContext.GetNamedResources() };

        auto depthSrv = rpi.GetInputAttachmentSRV(0, TextureViewDesc{{TextureViewDesc::Aspect::Depth}});
        if (!depthSrv || !depthSrv->IsGood()) return;

        TRY
        {
                // note -- we might need access to the MSAA defines for this shader
            auto& shaderProgram = ::Assets::Legacy::GetAssetDep<Metal::ShaderProgram>(
                BASIC2D_VERTEX_HLSL ":fullscreen_viewfrustumvector:vs_*",
                "xleres/ui/terrainmanipulators.hlsl:ps_circlehighlight:ps_*");
            
            struct HighlightParameters
            {
                Float3 _center;
                float _radius;
            } highlightParameters = { centre, radius };
            ConstantBufferView constantBufferPackets[2];
            constantBufferPackets[0] = MakeSharedPkt(highlightParameters);

            auto& circleHighlight = *::Assets::MakeAsset<RenderCore::Techniques::DeferredShaderResource>("xleres/DefaultResources/circlehighlight.png:L")->Actualize();
            const Metal::ShaderResourceView* resources[] = { depthSrv, &circleHighlight.GetShaderResource() };

			UniformsStreamInterface usi;
			usi.BindConstantBuffer(0, {Hash64("CircleHighlightParameters")});
            usi.BindShaderResource(0, Hash64("DepthTexture"));
            usi.BindShaderResource(1, Hash64("HighlightResource"));

			Metal::BoundUniforms boundLayout(
				shaderProgram,
				{},
				RenderCore::Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
				usi);

			auto& metalContext = *Metal::DeviceContext::Get(threadContext);
            metalContext.Bind(shaderProgram);
            boundLayout.Apply(metalContext, 0, parserContext.GetGlobalUniformsStream());
			boundLayout.Apply(metalContext, 1, UniformsStream{
				MakeIteratorRange(constantBufferPackets),
				UniformsStream::MakeResources(MakeIteratorRange(resources))
				});

            metalContext.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
            metalContext.Bind(Techniques::CommonResources()._dssDisable);
            metalContext.Bind(Topology::TriangleStrip);
			metalContext.UnbindInputLayout();

                // note --  this will render a full screen quad. we could render cylinder geometry instead,
                //          because this decal only affects the area within a cylinder. But it's just for
                //          tools, so the easy way should be fine.
            metalContext.Draw(4);

            SceneEngine::MetalStubs::UnbindPS<Metal::ShaderResourceView>(metalContext, 3, 1);
        } 
        CATCH_ASSETS(parserContext)
        CATCH(...) {} 
        CATCH_END
    }

    void RenderRectangleHighlight(
        IThreadContext& threadContext, 
        Techniques::ParsingContext& parserContext,
        const Float3& mins, const Float3& maxs,
		RectangleHighlightType type)
    {
		std::vector<FrameBufferDesc::Attachment> attachments {
			{ Techniques::AttachmentSemantics::ColorLDR, AsAttachmentDesc(parserContext.GetTechniqueContext()._attachmentPool->GetBoundResource(RenderCore::Techniques::AttachmentSemantics::ColorLDR)->GetDesc()) },
			{ Techniques::AttachmentSemantics::MultisampleDepth, Format::D24_UNORM_S8_UINT }
		};
		SubpassDesc mainPass;
		mainPass.SetName("RenderCylinderHighlight");
		mainPass.AppendOutput(0);
		mainPass.AppendInput(1);
		FrameBufferDesc fbDesc{ std::move(attachments), {mainPass} };
		Techniques::RenderPassInstance rpi {
			threadContext, fbDesc, 
			parserContext.GetFrameBufferPool(),
			parserContext.GetNamedResources() };

        auto depthSrv = rpi.GetInputAttachmentSRV(0, TextureViewDesc{{TextureViewDesc::Aspect::Depth}});
        if (!depthSrv || !depthSrv->IsGood()) return;

        TRY
        {
                // note -- we might need access to the MSAA defines for this shader
            auto& shaderProgram = ::Assets::Legacy::GetAssetDep<Metal::ShaderProgram>(
                BASIC2D_VERTEX_HLSL ":fullscreen_viewfrustumvector:vs_*",
                (type == RectangleHighlightType::Tool)
					? "xleres/ui/terrainmanipulators.hlsl:ps_rectanglehighlight:ps_*"
					: "xleres/ui/terrainmanipulators.hlsl:ps_lockedareahighlight:ps_*");
            
            struct HighlightParameters
            {
                Float3 _mins; float _dummy0;
                Float3 _maxs; float _dummy1;
            } highlightParameters = {
                mins, 0.f, maxs, 0.f
            };
            ConstantBufferView constantBufferPackets[2];
            constantBufferPackets[0] = MakeSharedPkt(highlightParameters);

            auto& circleHighlight = *::Assets::MakeAsset<RenderCore::Techniques::DeferredShaderResource>("xleres/DefaultResources/circlehighlight.png:L")->Actualize();
            const Metal::ShaderResourceView* resources[] = { depthSrv, &circleHighlight.GetShaderResource() };

			UniformsStreamInterface usi;
			usi.BindConstantBuffer(0, {Hash64("RectangleHighlightParameters")});
            usi.BindShaderResource(0, Hash64("DepthTexture"));
            usi.BindShaderResource(1, Hash64("HighlightResource"));

			Metal::BoundUniforms boundLayout(
				shaderProgram,
				{},
				RenderCore::Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
				usi);

            auto& metalContext = *RenderCore::Metal::DeviceContext::Get(threadContext);
			metalContext.Bind(shaderProgram);
			boundLayout.Apply(metalContext, 0, parserContext.GetGlobalUniformsStream());
			boundLayout.Apply(metalContext, 1, UniformsStream{
				MakeIteratorRange(constantBufferPackets),
				UniformsStream::MakeResources(MakeIteratorRange(resources))
				});

            metalContext.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
            metalContext.Bind(Techniques::CommonResources()._dssDisable);
            metalContext.Bind(Topology::TriangleStrip);
			#if GFXAPI_TARGET == GFXAPI_DX11
				metalContext.GetUnderlying()->IASetInputLayout(nullptr);
			#endif

                // note --  this will render a full screen quad. we could render cylinder geometry instead,
                //          because this decal only affects the area within a cylinder. But it's just for
                //          tools, so the easy way should be fine.
            metalContext.Draw(4);

            SceneEngine::MetalStubs::UnbindPS<Metal::ShaderResourceView>(metalContext, 3, 1);
        } 
        CATCH_ASSETS(parserContext)
        CATCH(...) {} 
        CATCH_END
    }

    class ManipulatorResBox
    {
    public:
        class Desc {};

        const ::Assets::DependencyValidation& GetDependencyValidation() { return _depVal; }

        FixedFunctionModel::SimpleShaderVariationManager _materialGenCylinder;

        ManipulatorResBox(const Desc&);
    private:
        ::Assets::DependencyValidation _depVal;
    };

    ManipulatorResBox::ManipulatorResBox(const Desc&)
    : _materialGenCylinder(
        InputLayout((const InputElementDesc*)nullptr, 0),
        { Techniques::ObjectCB::LocalTransform, Techniques::ObjectCB::BasicMaterialConstants },
        ParameterBox({ std::make_pair("SHAPE", "4") }))
    {
    }

    void DrawWorldSpaceCylinder(
        RenderCore::IThreadContext& threadContext, Techniques::ParsingContext& parserContext,
        Float3 origin, Float3 axis, float radius)
    {
        CATCH_ASSETS_BEGIN
            auto& box = ConsoleRig::FindCachedBoxDep2<ManipulatorResBox>();
            auto localToWorld = Identity<Float4x4>();
            SetTranslation(localToWorld, origin);
            SetUp(localToWorld, axis);

            Float3 forward = Float3(0.f, 0.f, 1.f);
            Float3 right = Cross(forward, axis);
            if (XlAbs(MagnitudeSquared(right)) < 1e-10f)
                right = Cross(Float3(0.f, 1.f, 0.f), axis);
            right = Normalize(right);
            Float3 adjustedForward = Normalize(Cross(axis, right));
            SetForward(localToWorld, radius * adjustedForward);
            SetRight(localToWorld, radius * right);

            auto shader = box._materialGenCylinder.FindVariation(
                parserContext, Techniques::TechniqueIndex::Forward, 
                AREA_LIGHT_TECH);
            
            if (shader._shader._shaderProgram) {
                auto& metalContext = *Metal::DeviceContext::Get(threadContext);
                ParameterBox matParams;
                matParams.SetParameter("MaterialDiffuse", Float3(0.03f, 0.03f, .33f));
                matParams.SetParameter("Opacity", 0.125f);
                auto transformPacket = Techniques::MakeLocalTransformPacket(
                    localToWorld, ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld));
                shader._shader.Apply(
					metalContext, parserContext, {});

				ConstantBufferView cbvs[] = { transformPacket, shader._cbLayout->BuildCBDataAsPkt(matParams, RenderCore::Techniques::GetDefaultShaderLanguage()) };
				shader._shader._boundUniforms->Apply(metalContext, 1, { MakeIteratorRange(cbvs) });

                auto& commonRes = Techniques::CommonResources();
                metalContext.Bind(commonRes._blendStraightAlpha);
                metalContext.Bind(commonRes._dssReadOnly);
                // metalContext.Unbind<Metal::VertexBuffer>();
                metalContext.Bind(Topology::TriangleList);
                
                const unsigned vertexCount = 32 * 6;	// (must agree with the shader!)
                metalContext.Draw(vertexCount);
            }

        CATCH_ASSETS_END(parserContext)
    }

    void DrawQuadDirect(
        RenderCore::IThreadContext& threadContext, const RenderCore::Metal::ShaderResourceView& srv, 
        Float2 screenMins, Float2 screenMaxs)
    {
        auto& metalContext = *RenderCore::Metal::DeviceContext::Get(threadContext);

        class Vertex
        {
        public:
            Float2  _position;
            Float2  _texCoord;
        } vertices[] = {
            { Float2(screenMins[0], screenMins[1]), Float2(0.f, 0.f) },
            { Float2(screenMins[0], screenMaxs[1]), Float2(0.f, 1.f) },
            { Float2(screenMaxs[0], screenMins[1]), Float2(1.f, 0.f) },
            { Float2(screenMaxs[0], screenMaxs[1]), Float2(1.f, 1.f) }
        };

        InputElementDesc vertexInputLayout[] = {
            InputElementDesc( "POSITION", 0, Format::R32G32_FLOAT ),
            InputElementDesc( "TEXCOORD", 0, Format::R32G32_FLOAT )
        };

        auto vertexBuffer = Metal::MakeVertexBuffer(Metal::GetObjectFactory(), MakeIteratorRange(vertices));

        const auto& shaderProgram = ::Assets::Legacy::GetAssetDep<Metal::ShaderProgram>(
            BASIC2D_VERTEX_HLSL ":P2T:" VS_DefShaderModel, 
            BASIC_PIXEL_HLSL ":copy_bilinear:" PS_DefShaderModel);
        Metal::BoundInputLayout boundVertexInputLayout(MakeIteratorRange(vertexInputLayout), shaderProgram);
		VertexBufferView vbvs[] = {&vertexBuffer};
        boundVertexInputLayout.Apply(metalContext, MakeIteratorRange(vbvs));
        metalContext.Bind(shaderProgram);

        Metal::ViewportDesc viewport = metalContext.GetBoundViewport();
        float constants[] = { 1.f / viewport.Width, 1.f / viewport.Height, 0.f, 0.f };
        auto reciprocalViewportDimensions = MakeSharedPkt(constants);
        const Metal::ShaderResourceView* resources[] = { &srv };
        ConstantBufferView cbvs[] = { reciprocalViewportDimensions };
		UniformsStreamInterface interf;
		interf.BindConstantBuffer(0, {Hash64("ReciprocalViewportDimensionsCB")});
        interf.BindShaderResource(0, Hash64("DiffuseTexture"));

		Metal::BoundUniforms boundLayout(shaderProgram, Metal::PipelineLayoutConfig{}, {}, interf);
		boundLayout.Apply(metalContext, 1, {MakeIteratorRange(cbvs), UniformsStream::MakeResources(MakeIteratorRange(resources))});

        metalContext.Bind(Metal::BlendState(BlendOp::Add, Blend::SrcAlpha, Blend::InvSrcAlpha));
        metalContext.Bind(Topology::TriangleStrip);
        metalContext.Draw(dimof(vertices));

		boundLayout.UnbindShaderResources(metalContext, 1);
    }
}

