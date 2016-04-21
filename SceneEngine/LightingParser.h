// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/IThreadContext_Forward.h"
#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/Metal/TextureView.h"
#include "../BufferUploads/IBufferUploads_Forward.h"
#include "../Math/Matrix.h"
#include <functional>

namespace RenderCore { namespace Techniques { class CameraDesc; class ProjectionDesc; } }

namespace SceneEngine
{
    using MetalContext = RenderCore::Metal::DeviceContext;

    namespace ShaderLightDesc { class BasicEnvironment; }

    class RenderingQualitySettings
    {
    public:
        UInt2       _dimensions;
        unsigned    _samplingCount, _samplingQuality;

        enum class LightingModel
        {
            Forward,
            Deferred
        };

        LightingModel _lightingModel;

        RenderingQualitySettings();
        RenderingQualitySettings(
            UInt2 dimensions,
            LightingModel lightingModel = LightingModel::Deferred,
            unsigned samplingCount = 1,
            unsigned samplingQuality = 0);
    };

    class LightingParserContext;
    class SceneParseSettings;
    class ISceneParser;
    class AttachedSceneMarker;
    class PreparedScene;

    /// <summary>Execute rendering</summary>
    /// This is the main entry point for rendering a scene.
    /// The lighting parser will organize buffers, perform lighting resolve
    /// operations and call out to the scene parser when parts of the scene
    /// need to be rendered. Typically this is called once per frame (though
    /// perhaps there are times when multiple renders are required for a frame,
    /// maybe for reflections).
    ///
    /// Note that the lighting parser will write the final result to the render
    /// target that is currently bound to the given context! Often, this will
    /// be the main back buffer. Usually, the width and height in "qualitySettings"
    /// should be the same dimensions as this output buffer (but that doesn't 
    /// always have to be the case).
    ///
    /// The "qualitySettings" parameter allows the caller to define the resolution
    /// and sampling quality for rendering the scene. Be careful to select valid 
    /// settings for sampling quality.
    ///
    /// Basic usage:
    /// <code>
    ///     auto renderDevice = RenderCore::CreateDevice();
    ///     auto presentationChain = renderDevice->CreatePresentationChain(...);
    ///     LightingParserContext lightingParserContext(...);
    ///     renderDevice->BeginFrame(presentationChain.get());
    ///
    ///     SceneEngine::RenderingQualitySettings qualitySettings;
    ///     auto presChainDesc = presentationChain->GetDesc();
    ///     qualitySettings._width = presChainDesc._width;
    ///     qualitySettings._height = presChainDesc._height;
    ///     qualitySettings._samplingCount = 1; 
    ///     qualitySettings._samplingQuality = 0;
    ///
    ///     auto context = RenderCore::Metal::DeviceContext::GetImmediateContext(renderDevice.get());
    ///     SceneEngine::LightingParser_Execute(context, lightingParserContext, qualitySettings);
    ///
    ///     presentationChain->Present();
    /// </code>
    void LightingParser_ExecuteScene(
        RenderCore::IThreadContext& context,
        LightingParserContext& parserContext,
        ISceneParser& sceneParser,
        const RenderCore::Techniques::CameraDesc& camera,
        const RenderingQualitySettings& qualitySettings);

    /// <summary>Executes the scene currently set to the parser context</summary>
    /// The currently attached scene will be rendered.
    /// Call LightingParser_SetupScene to attach a scene to the parser context before
    /// calling this.
    /// The other version of LightingParser_ExecuteScene is intended for most cases. But
    /// this version can be used for special case purpose (for example, when a special
    /// case projection matrix is required)
    void LightingParser_ExecuteScene(
        RenderCore::IThreadContext& metalContext, 
        LightingParserContext& parserContext,
        const RenderingQualitySettings& qualitySettings,
        PreparedScene& preparedScene);

    /// <summary>Initialise basic states for scene rendering</summary>
    /// Some render operations don't want to use the full lighting parser structure.
    /// In these cases, you can use LightingParser_SetupScene() to initialise the
    /// global states that are normally managed by the lighting parser.
    /// Note -- don't call this if you're using LightingParser_Execute.
    /// <seealso cref="LightingParser_Execute"/>
    auto LightingParser_SetupScene(
        MetalContext& context,
        LightingParserContext& parserContext,
        ISceneParser* sceneParser = nullptr,
        unsigned samplingPassIndex = 0, unsigned samplingPassCount = 1)
        -> AttachedSceneMarker;

    /// <summary>Set camera related states after camera changes</summary>
    /// Normally this is called automatically by the system.
    /// But in cases where you need to change the camera settings, you can
    /// manually force an update of the shader constants related to projection
    /// with this call.
    /// (for example, used by the vegetation spawn to temporarily reduce the
    /// far clip distance)
    /// <seealso cref="LightingParser_SetupScene"/>
    void LightingParser_SetGlobalTransform( 
        MetalContext& context, 
        LightingParserContext& parserContext, 
        const RenderCore::Techniques::ProjectionDesc& projDesc);

    /// <summary>Build a projection desc with parameters from a standard camera</summary>
    RenderCore::Techniques::ProjectionDesc BuildProjectionDesc(
        const RenderCore::Techniques::CameraDesc& sceneCamera,
        UInt2 viewportDims, 
        const Float4x4* specialProjectionMatrix = nullptr);

    /// <summary>Build a projection desc for an orthogonal camera</summary>
    RenderCore::Techniques::ProjectionDesc BuildProjectionDesc(
        const Float4x4& cameraToWorld,
        float l, float t, float r, float b,
        float nearClip, float farClip);

    void SetFrameGlobalStates(MetalContext& context);
    void ReturnToSteadyState(MetalContext& context);

        ///////////////////////////////////////////////////////////////////////////

    class MainTargets;

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
        MainTargets& GetMainTargets() const;

        typedef void ResolveFn(MetalContext*, LightingParserContext&, LightingResolveContext&, unsigned resolvePass);
        void        AppendResolve(std::function<ResolveFn>&& fn);
        void        SetPass(Pass::Enum newPass);

            //  The following are bound resources used by the ambient resolve shader
            //  In this way, we can do the resolve for all of these effects in one step
            //  (rather than having to perform a bunch of separate passes)
            //  But it means we need some special case handling for these resources.
        RenderCore::Metal::ShaderResourceView      _tiledLightingResult;
        RenderCore::Metal::ShaderResourceView      _ambientOcclusionResult;
        RenderCore::Metal::ShaderResourceView      _screenSpaceReflectionsResult;

        std::vector<std::function<ResolveFn>>       _queuedResolveFunctions;

        LightingResolveContext(MainTargets& mainTargets);
        ~LightingResolveContext();
    private:
        unsigned _samplingCount;
        bool _useMsaaSamplers;
        Pass::Enum _pass;
        MainTargets* _mainTargets;
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
            RenderCore::IThreadContext&, LightingParserContext&, PreparedScene&) const = 0;

        virtual void OnLightingResolvePrepare(
            MetalContext&, LightingParserContext&, LightingResolveContext&) const = 0;

        virtual void OnPostSceneRender(
            MetalContext&, LightingParserContext&, 
            const SceneParseSettings&, unsigned techniqueIndex) const = 0;

        virtual void InitBasicLightEnvironment(
            MetalContext&, LightingParserContext&, ShaderLightDesc::BasicEnvironment& env) const = 0;
    };
}

