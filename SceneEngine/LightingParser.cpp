// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LightingParser.h"
#include "LightingParserContext.h"
#include "SceneParser.h"
#include "SceneEngineUtils.h"
#include "RenderStepUtil.h"

#include "LightingTargets.h"
#include "LightInternal.h"
#include "Tonemap.h"
#include "VolumetricFog.h"
#include "ShadowResources.h"
#include "MetricsBox.h"
#include "Ocean.h"
#include "DeepOceanSim.h"
#include "RefractionsBuffer.h"
#include "OrderIndependentTransparency.h"
#include "StochasticTransparency.h"
#include "DepthWeightedTransparency.h"
#include "Sky.h"
#include "SunFlare.h"
#include "Rain.h"
#include "RayTracedShadows.h"
#include "MetalStubs.h"

#include "../RenderCore/RenderUtils.h"
#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/RenderStateResolver.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/DeferredShaderResource.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/QueryPool.h"
#include "../RenderCore/IAnnotator.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../Math/Transformations.h"
#include "../Math/ProjectionMath.h"
#include "../Utility/FunctionUtils.h"

#include <map>

#if GFXAPI_ACTIVE == GFXAPI_DX11
    #include "../RenderCore/DX11/Metal/IncludeDX11.h"
#endif

// temporary warning disable
#pragma warning(disable:4189)   // 'hasOITrans' : local variable is initialized but not referenced
#pragma warning(disable:4505)   // 'SceneEngine::LightingParser_ResolveMSAA' : unreferenced local function has been removed

namespace SceneEngine
{
    using namespace RenderCore;
    using SPS = SceneParseSettings;

    DeepOceanSimSettings GlobalOceanSettings; 
    OceanLightingSettings GlobalOceanLightingSettings; 

#if 0
    class FrameBufferDescBox
    {
    public:
        class Desc
        {
        public:
            TextureSamples      _samples;
            bool                _precisionTargets;
            unsigned            _gbufferMode;
            Desc(const TextureSamples& samples, bool precisionTargets, unsigned gbufferMode) 
            {
                std::fill((char*)this, PtrAdd((char*)this, sizeof(*this)), '\0');
                _samples = samples;
                _precisionTargets = precisionTargets;
                _gbufferMode = gbufferMode;
            }
        };

        FrameBufferDesc _createGBuffer;
        std::map<AttachmentName, AttachmentDesc> _attachments;

        FrameBufferDescBox(const Desc& d);
    };

    // static const RenderPassFragment::SystemName s_renderToGBuffer = 1u;
    // static const RenderPassFragment::SystemName s_resolveLighting = 2u;

    FrameBufferDescBox::FrameBufferDescBox(const Desc& desc)
    {
        
    }

    class MainTargets : public IMainTargets
    {
    public:
        unsigned                        GetGBufferType() const;
        RenderCore::TextureSamples      GetSampling() const;
        const RenderSceneSettings&		GetRenderSceneSettings() const;
        UInt2							GetDimensions() const;
		const SRV&                      GetSRV(Name, const TextureViewDesc& window = {}) const;
        bool                            HasSRV(Name) const;

        MainTargets(
            Techniques::ParsingContext& parsingContext,
            const RenderSceneSettings& qualSettings,
            unsigned gbufferType);
        ~MainTargets();

    private:
        Techniques::ParsingContext* _parsingContext;
        RenderSceneSettings			_qualSettings;
        unsigned                    _gbufferType;
    };

    unsigned                        MainTargets::GetGBufferType() const
    {
        return _gbufferType;
    }

    RenderCore::TextureSamples      MainTargets::GetSampling() const
    {
        return TextureSamples::Create(
            uint8(std::max(_qualSettings._samplingCount, 1u)), uint8(_qualSettings._samplingQuality));
    }

    const RenderSceneSettings& MainTargets::GetRenderSceneSettings() const
    {
        return _qualSettings;
    }

    UInt2 MainTargets::GetDimensions() const
    {
        return _qualSettings._dimensions;
    }

    auto  MainTargets::GetSRV(Name name, const TextureViewDesc& window) const -> const SRV&
    {
        auto result = _parsingContext->GetNamedResources().GetSRV(name, window);
        assert(result);
        return *result;
    }

    bool MainTargets::HasSRV(Name name) const
    {
        return _parsingContext->GetNamedResources().GetSRV(name) != nullptr;
    }

    MainTargets::MainTargets(
        Techniques::ParsingContext& parsingContext,
        const RenderSceneSettings& qualSettings,
        unsigned gbufferType)
    : _parsingContext(&parsingContext)
    , _qualSettings(qualSettings)
    , _gbufferType(gbufferType)
    {}

    MainTargets::~MainTargets() {}
#endif

    void LightingParser_MainScene(
        IThreadContext& context,
        Metal::DeviceContext& metalContext, 
		Techniques::ParsingContext& parserContext,
        LightingParserContext& lightingParserContext,
        const RenderSceneSettings& qualitySettings)
    {
            ////////////////////////////////////////////////////////////////////
                //      1. Resolve lighting
                //          -> outputs  1. postLightingResolveTexture (HDR colour)
                //                      2. depth buffer (NDC depths)
                //                      3. secondary depth buffer (contains per 
                //                          pixel/per sample stencil)
                //
            ////////............................................................
                //      2. Resolve MSAA (if needed)
                //          -> outputs  1. single sample HDR colour
                //                      2. single sample depth buffer
                //
            ////////............................................................
                //      3. Post processing operations
                //
            ////////............................................................
                //      4. Resolve HDR (tone mapping, bloom, etc)
                //          -> outputs  1. LDR SRGB colour
                //
            ////////............................................................
                //      5. Debugging / overlays
                //
            ////////////////////////////////////////////////////////////////////

        const bool precisionTargets = Tweakable("PrecisionTargets", false);

        if (qualitySettings._lightingModel == RenderSceneSettings::LightingModel::Deferred) {

            // RenderStep_GBuffer

        } else if (qualitySettings._lightingModel == RenderSceneSettings::LightingModel::Forward) {

#if 0   // platformtemp
            auto& mainTargets = Techniques::FindCachedBox2<ForwardTargetsBox>(
                unsigned(mainViewport.Width), unsigned(mainViewport.Height),
                FormatStack(NativeFormat(DXGI_FORMAT_R24G8_TYPELESS), 
                            NativeFormat(DXGI_FORMAT_R24_UNORM_X8_TYPELESS), 
                            NativeFormat(DXGI_FORMAT_D24_UNORM_S8_UINT)),
                sampling);

            metalContext.Clear(mainTargets._msaaDepthBuffer, 1.f, 0);
            metalContext.Bind(
                MakeResourceList(lightingResTargets._lightingResolveRTV),
                &mainTargets._msaaDepthBuffer);

            if (!parserContext._preparedDMShadows.empty())
                BindShadowsForForwardResolve(metalContext, parserContext, parserContext._preparedDMShadows[0].second);

            const auto sampleCount = mainTargets.GetSampling()._sampleCount;
			ForwardLightingModel_Render(context, parserContext, lightingParserContext, preparedScene, sampleCount);
#endif

        } else if (qualitySettings._lightingModel == RenderSceneSettings::LightingModel::Direct) {

			auto rpi = RenderPassToPresentationTarget(context, parserContext);
			metalContext.Bind(Metal::ViewportDesc{0.f, 0.f, (float)qualitySettings._dimensions[0], (float)qualitySettings._dimensions[1]});

			

		}

        {
            // RenderStep_ResolveHDR
        }
    }

    void LightingParser_InitBasicLightEnv(  
        RenderCore::IThreadContext& context,
        Techniques::ParsingContext& parserContext,
		LightingParserContext& lightingParserContext);

    static LightingParserContext LightingParser_SetupContext(
        RenderCore::IThreadContext& context, 
		RenderCore::Techniques::ParsingContext& parserContext,
        ILightingParserDelegate& delegate,
		const RenderSceneSettings& qualitySettings,
        unsigned samplingPassIndex = 0, unsigned samplingPassCount = 1)
    {
        struct GlobalCBuffer
        {
            float _time; unsigned _samplingPassIndex; 
            unsigned _samplingPassCount; unsigned _dummy;
        } time { delegate.GetTimeValue(), samplingPassIndex, samplingPassCount, 0 };
		auto& metalContext = *Metal::DeviceContext::Get(context);
        parserContext.SetGlobalCB(
            metalContext, Techniques::TechniqueContext::CB_GlobalState,
            &time, sizeof(time));

		LightingParserContext lightingParserContext;
		lightingParserContext._delegate = &delegate;
		lightingParserContext._plugins.insert(
			lightingParserContext._plugins.end(),
			qualitySettings._lightingPlugins.begin(), qualitySettings._lightingPlugins.end());
        LightingParser_InitBasicLightEnv(context, parserContext, lightingParserContext, delegate);

        auto& metricsBox = ConsoleRig::FindCachedBox2<MetricsBox>();
        metalContext.ClearUInt(metricsBox._metricsBufferUAV, { 0,0,0,0 });
        lightingParserContext.SetMetricsBox(&metricsBox);

        return lightingParserContext;
    }

	class SceneExecuteContext_Main : public SceneExecuteContext
	{
	public:
		unsigned _shadowViewCount;
		PreparedScene _preparedScene;
	};

	void LightingParser_PrepareShadows(
        IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parsingContext,
        LightingParserContext& lightingParserContext, 
		SceneExecuteContext_Main& executedScene, PreparedScene& preparedScene, IMainTargets& mainTargets);

    static void LightingParser_Render(
        RenderCore::IThreadContext& context, 
		RenderCore::IResource& renderTarget,
        Techniques::ParsingContext& parserContext,
		LightingParserContext& lightingParserContext,
        const RenderSceneSettings& qualitySettings,
		SceneExecuteContext_Main& executedScene)
    {
        auto metalContext = Metal::DeviceContext::Get(context);
        CATCH_ASSETS_BEGIN
            ReturnToSteadyState(*metalContext);
            SetFrameGlobalStates(*metalContext);
        CATCH_ASSETS_END(parserContext)

        const bool enableParametersBuffer = Tweakable("EnableParametersBuffer", true);
        MainTargets mainTargets(parserContext, qualitySettings, enableParametersBuffer?1:2);

        {
            GPUAnnotation anno(context, "Prepare");
            for (auto i=lightingParserContext._plugins.cbegin(); i!=lightingParserContext._plugins.cend(); ++i) {
                CATCH_ASSETS_BEGIN
                    (*i)->OnPreScenePrepare(context, parserContext, lightingParserContext, sceneParser, preparedScene);
                CATCH_ASSETS_END(parserContext)
            }

            LightingParser_PrepareShadows(context, parserContext, lightingParserContext, &sceneParser, preparedScene, mainTargets);
        }

        GetBufferUploads().Update(context, true);

        CATCH_ASSETS_BEGIN
            LightingParser_MainScene(context, *metalContext, parserContext, lightingParserContext, sceneParser, preparedScene, mainTargets, qualitySettings);
        CATCH_ASSETS_END(parserContext)
    }

	LightingParserContext LightingParser_ExecuteScene(
        RenderCore::IThreadContext& context, 
		RenderCore::IResource& renderTarget,
        Techniques::ParsingContext& parserContext,
        IScene& scene,
		ILightingParserDelegate& delegate,
        const RenderCore::Techniques::CameraDesc& camera,
        const RenderSceneSettings& renderSettings)
    {
		SceneExecuteContext_Main executeContext;
        scene.ExecuteScene(context, executeContext);

        // Throw in a "frame priority barrier" here, right after the prepare scene. This will
        // force all uploads started during PrepareScene to be completed when we next call
        // bufferUploads.Update(). Normally we want a gap between FramePriority_Barrier() and
        // Update(), because this gives some time for the background thread to run.
        GetBufferUploads().FramePriority_Barrier();

		auto lightingParserContext = LightingParser_SetupContext(context, parserContext, delegate, renderSettings);
        LightingParser_SetGlobalTransform(
            context, parserContext, 
            RenderCore::Techniques::BuildProjectionDesc(camera, renderSettings._dimensions));
        LightingParser_Render(context, renderTarget, parserContext, lightingParserContext, renderSettings, executeContext);

		return lightingParserContext;
    }

    void LightingParserContext::SetMetricsBox(MetricsBox* box)
    {
        _metricsBox = box;
    }

    void LightingParserContext::Reset()
    {
        _preparedDMShadows.clear();
        _preparedRTShadows.clear();
    }

	LightingParserContext::LightingParserContext() {}
	LightingParserContext::~LightingParserContext() {}
	LightingParserContext::LightingParserContext(LightingParserContext&& moveFrom)
	: _preparedDMShadows(std::move(moveFrom._preparedDMShadows))
	, _preparedRTShadows(std::move(moveFrom._preparedRTShadows))
	, _plugins(std::move(moveFrom._plugins))
	, _metricsBox(moveFrom._metricsBox)
	{
		moveFrom._metricsBox = nullptr;
	}

	LightingParserContext& LightingParserContext::operator=(LightingParserContext&& moveFrom)
	{
		_preparedDMShadows = std::move(moveFrom._preparedDMShadows);
		_preparedRTShadows = std::move(moveFrom._preparedRTShadows);
		_plugins = std::move(moveFrom._plugins);
		_metricsBox = moveFrom._metricsBox;
		moveFrom._metricsBox = nullptr;
		return *this;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void LightingParser_Overlays(   IThreadContext& context,
									Techniques::ParsingContext& parserContext,
                                    LightingParserContext& lightingParserContext)
    {
        GPUAnnotation anno(context, "Overlays");

		auto metalContext = Metal::DeviceContext::Get(context);
        Metal::ViewportDesc mainViewportDesc(*metalContext);
        auto& refractionBox = ConsoleRig::FindCachedBox2<RefractionsBuffer>(unsigned(mainViewportDesc.Width/2), unsigned(mainViewportDesc.Height/2));
        refractionBox.Build(*metalContext, parserContext, 4.f);
        MetalStubs::GetGlobalNumericUniforms(*metalContext, ShaderStage::Pixel).Bind(MakeResourceList(12, refractionBox.GetSRV()));

        for (auto i=parserContext._pendingOverlays.cbegin(); i!=parserContext._pendingOverlays.cend(); ++i) {
            CATCH_ASSETS_BEGIN
                (*i)(*metalContext, parserContext);
            CATCH_ASSETS_END(parserContext)
        }
                    
        if (Tweakable("FFTDebugging", false)) {
            FFT_DoDebugging(metalContext.get());
        }

        if (Tweakable("MetricsRender", false) && lightingParserContext.GetMetricsBox()) {
            CATCH_ASSETS_BEGIN

                using namespace RenderCore;
                using namespace RenderCore::Metal;
                auto& metricsShader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                        "xleres/utility/metricsrender.vsh:main:vs_*", 
                        "xleres/utility/metricsrender.gsh:main:gs_*",
                        "xleres/utility/metricsrender.psh:main:ps_*",
                        "");
                metalContext->Bind(metricsShader);
                metalContext->GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(
                    3, ::Assets::MakeAsset<RenderCore::Techniques::DeferredShaderResource>("xleres/DefaultResources/metricsdigits.dds:T")->Actualize()->GetShaderResource()));
                metalContext->Bind(BlendState(BlendOp::Add, Blend::One, Blend::InvSrcAlpha));
                metalContext->Bind(DepthStencilState(false));
                metalContext->GetNumericUniforms(ShaderStage::Vertex).Bind(MakeResourceList(lightingParserContext.GetMetricsBox()->_metricsBufferSRV));
                unsigned dimensions[4] = { unsigned(mainViewportDesc.Width), unsigned(mainViewportDesc.Height), 0, 0 };
                metalContext->GetNumericUniforms(ShaderStage::Vertex).Bind(MakeResourceList(MakeMetalCB(dimensions, sizeof(dimensions))));
                metalContext->GetNumericUniforms(ShaderStage::Geometry).Bind(MakeResourceList(MakeMetalCB(dimensions, sizeof(dimensions))));
                SetupVertexGeneratorShader(*metalContext);
                metalContext->Bind(Topology::PointList);
                metalContext->Draw(9);

                MetalStubs::UnbindPS<ShaderResourceView>(*metalContext, 3, 1);
                MetalStubs::UnbindVS<ShaderResourceView>(*metalContext, 0, 1);

            CATCH_ASSETS_END(parserContext)
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    IScene::~IScene() {}
	ILightingParserDelegate::~ILightingParserDelegate() {}

}

