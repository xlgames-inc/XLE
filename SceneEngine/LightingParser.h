// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/IThreadContext_Forward.h"
#include "../RenderCore/IDevice_Forward.h"
#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/Metal/TextureView.h"
#include "../BufferUploads/IBufferUploads_Forward.h"
#include "../Math/Matrix.h"
#include "../Math/Vector.h"
#include "../Utility/IteratorUtils.h"
#include <functional>

namespace RenderCore { namespace Techniques { class CameraDesc; class ProjectionDesc; class ParsingContext; enum class BatchFilter; } }

namespace SceneEngine
{
	class LightingParserContext;
    class IScene;
    class PreparedScene;
	class ILightingParserDelegate;
	class ILightingParserPlugin;

    namespace ShaderLightDesc { class BasicEnvironment; }

    class SceneTechniqueDesc
    {
    public:
        enum class LightingModel
        {
            Forward,
            Deferred,
			Direct
        };

        LightingModel _lightingModel = LightingModel::Deferred;

		const ILightingParserDelegate* _lightingDelegate;
		IteratorRange<const std::shared_ptr<ILightingParserPlugin>*> _lightingPlugins = {};

		unsigned    _samplingCount = 1u;
		unsigned	_samplingQuality = 0u;
    };

    /// <summary>Execute rendering</summary>
    /// This is the main entry point for rendering a scene.
    /// The lighting parser will organize buffers, perform lighting resolve
    /// operations and call out to the scene parser when parts of the scene
    /// need to be rendered. Typically this is called once per frame (though
    /// perhaps there are times when multiple renders are required for a frame,
    /// maybe for reflections).
    ///
    /// The "qualitySettings" parameter allows the caller to define the resolution
    /// and sampling quality for rendering the scene. Be careful to select valid 
    /// settings for sampling quality.
    ///
    /// Basic usage:
    /// <code>
    ///     auto renderDevice = RenderCore::CreateDevice();
    ///     auto presentationChain = renderDevice->CreatePresentationChain(...);
    ///     RenderCore::Techniques::ParsingContext parsingContext(...);
	///		std::shared_ptr<SceneEngine::ISceneParser> scene = ...;
    ///     renderDevice->BeginFrame(presentationChain.get());
    ///
    ///     auto presChainDesc = presentationChain->GetDesc();
	///     SceneEngine::RenderSceneSettings qualitySettings {
	///			UInt2(presChainDesc._width, presChainDesc._height) };
	///
    ///     auto lightingParserContext = SceneEngine::LightingParser_Execute(
	///			renderDevice->GetImmediateContext(), 
	///			parsingContext, *scene, qualitySettings);
    ///
    ///     presentationChain->Present();
    /// </code>
    LightingParserContext LightingParser_ExecuteScene(
        RenderCore::IThreadContext& context,
		const RenderCore::IResourcePtr& renderTarget,
		RenderCore::Techniques::ParsingContext& parserContext,
		IScene& scene,
        const RenderCore::Techniques::CameraDesc& camera,
        const RenderSceneSettings& qualitySettings);

    void LightingParser_Overlays(
        RenderCore::IThreadContext& context,
		RenderCore::Techniques::ParsingContext& parserContext,
        LightingParserContext& lightingParserContext);

        ///////////////////////////////////////////////////////////////////////////

	class ShadowProjectionDesc;
	class GlobalLightingDesc;
	class LightDesc;
	class ToneMapSettings;
	class MainTargets;

	class ILightingParserDelegate
	{
	public:
        using ProjectionDesc    = RenderCore::Techniques::ProjectionDesc;
        using ShadowProjIndex   = unsigned;
        using LightIndex        = unsigned;

        virtual ShadowProjIndex GetShadowProjectionCount() const = 0;
        virtual auto            GetShadowProjectionDesc(ShadowProjIndex index, const ProjectionDesc& mainSceneProj) const
            -> ShadowProjectionDesc = 0;

        virtual LightIndex  GetLightCount() const = 0;
        virtual auto        GetLightDesc(LightIndex index) const -> const LightDesc& = 0;
        virtual auto        GetGlobalLightingDesc() const -> GlobalLightingDesc = 0;
        virtual auto        GetToneMapSettings() const -> ToneMapSettings = 0;

        virtual float       GetTimeValue() const = 0;

		virtual ~ILightingParserDelegate();
    };

		///////////////////////////////////////////////////////////////////////////

    /// <summary>The LightingResolveContext is used by lighting operations during the gbuffer resolve step</summary>
    /// Don't confuse with LightingParserContext. This is a different context object, representing
    /// a sub-step during the larger lighting parser process.
    /// During the lighting resolve step, we take the complete gbuffer, and generate the "lighting buffer"
    /// There are typically a number of steps to perform, for effects like reflections, indirect
    /// lighting and atmosphere attenuation
    class LightingResolveContext
    {
    public:
        struct Pass { enum Enum { PerSample, PerPixel, Prepare }; };

        Pass::Enum  GetCurrentPass() const;
        bool        UseMsaaSamplers() const;
        unsigned    GetSamplingCount() const;

        typedef void ResolveFn(RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&, LightingResolveContext&, unsigned resolvePass);
        void        AppendResolve(std::function<ResolveFn>&& fn);
        void        SetPass(Pass::Enum newPass);

            //  The following are bound resources used by the ambient resolve shader
            //  In this way, we can do the resolve for all of these effects in one step
            //  (rather than having to perform a bunch of separate passes)
            //  But it means we need some special case handling for these resources.
        RenderCore::Metal::ShaderResourceView		_tiledLightingResult;
        RenderCore::Metal::ShaderResourceView		_ambientOcclusionResult;
        RenderCore::Metal::ShaderResourceView		_screenSpaceReflectionsResult;

        std::vector<std::function<ResolveFn>>		_queuedResolveFunctions;

        LightingResolveContext(const LightingParserContext& lightingParserContext);
        ~LightingResolveContext();
    private:
        unsigned _samplingCount;
        bool _useMsaaSamplers;
        Pass::Enum _pass;
    };

    /// <summary>Plug-in for the lighting parser</summary>
    /// This allows for some customization of the lighting parser operations.
    /// <list>
    ///     <item>OnPreScenePrepare --  this will be executed before the main rendering process begins. 
    ///             It is needed for preparing resources that will be used in later steps of the pipeline.</item>
    ///     <item>OnLightingResolvePrepare -- this will be executed before the lighting resolve step 
    ///             begins. There are two purposes to this: to prepare any resources that will be required 
    ///             during lighting resolve, and to queue operations that should happen during lighting 
    ///             resolve. To queue operations, use LightingResolveContext::AppendResolve</item>
    /// </list>
    /// 
    class ILightingParserPlugin
    {
    public:
        virtual void OnPreScenePrepare(
            RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&) const;

        virtual void OnLightingResolvePrepare(
            RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&, 
			LightingResolveContext&) const;

        virtual void OnPostSceneRender(
            RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&, 
			RenderCore::Techniques::BatchFilter filter, unsigned techniqueIndex) const;

        virtual void InitBasicLightEnvironment(
            RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&, 
			ShaderLightDesc::BasicEnvironment& env) const;

		virtual ~ILightingParserPlugin();
    };
}

