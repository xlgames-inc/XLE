// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS         // suppress warning related to std::move (algorithm.h version)

#include "ScreenspaceReflections.h"
#include "SceneEngineUtility.h"
#include "ResourceBox.h"
#include "LightingParserContext.h"
#include "SceneParser.h"
#include "Techniques.h"
#include "CommonResources.h"
#include "Sky.h"
#include "LightDesc.h"

#include "../RenderCore/Metal/RenderTargetView.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../Math/Transformations.h"

#include "../ConsoleRig/Console.h"

#include "../Core/WinAPI/IncludeWindows.h"

namespace SceneEngine
{
    using namespace RenderCore;
    using namespace RenderCore::Metal;

    class ScreenSpaceReflectionsResources
    {
    public:
        class Desc
        {
        public:
            unsigned _width, _height;
            unsigned _downsampleScale;
            bool _useMsaaSamplers;
            unsigned _maskJitterRadius;
            bool _interpolateSamples;
            Desc(unsigned width, unsigned height, unsigned downsampleScale, bool useMsaaSamplers, unsigned maskJitterRadius, bool interpolateSamples) 
                : _useMsaaSamplers(useMsaaSamplers), _width(width), _height(height), _downsampleScale(downsampleScale), _maskJitterRadius(maskJitterRadius), _interpolateSamples(interpolateSamples) {}
        };

        intrusive_ptr<ID3D::Resource> _maskTexture;
        RenderCore::Metal::UnorderedAccessView _maskUnorderedAccess;
        RenderCore::Metal::ShaderResourceView _maskShaderResource;
        RenderCore::Metal::ComputeShader* _buildMask;

        intrusive_ptr<ID3D::Resource> _reflectionsTexture;
        RenderCore::Metal::RenderTargetView _reflectionsTarget;
        RenderCore::Metal::UnorderedAccessView _reflectionsUnorderedAccess;
        RenderCore::Metal::ShaderResourceView _reflectionsShaderResource;
        RenderCore::Metal::ComputeShader* _buildReflections;

        intrusive_ptr<ID3D::Resource> _downsampledNormals;
        intrusive_ptr<ID3D::Resource> _downsampledDepth;
        RenderCore::Metal::RenderTargetView _downsampledNormalsTarget;
        RenderCore::Metal::RenderTargetView _downsampledDepthTarget;
        RenderCore::Metal::ShaderResourceView _downsampledNormalsShaderResource;
        RenderCore::Metal::ShaderResourceView _downsampledDepthShaderResource;

        RenderCore::Metal::ShaderProgram* _downsampleTargets;
        RenderCore::Metal::ShaderProgram* _horizontalBlur;
        RenderCore::Metal::ShaderProgram* _verticalBlur;

        RenderCore::Metal::ConstantBuffer _samplingPatternConstants;

        Desc _desc;

        ScreenSpaceReflectionsResources(const Desc& desc);
        ~ScreenSpaceReflectionsResources();

        const Assets::DependencyValidation& GetDependencyValidation() const   { return *_validationCallback; }
    private:
        std::shared_ptr<Assets::DependencyValidation>  _validationCallback;
    };

    ScreenSpaceReflectionsResources::ScreenSpaceReflectionsResources(const Desc& desc)
    : _desc(desc)
    {
        using namespace RenderCore;
        using namespace RenderCore::Metal;
        using namespace BufferUploads;
        auto& uploads = *GetBufferUploads();
    
            ////////////
        auto maskTexture = uploads.Transaction_Immediate(
                BuildRenderTargetDesc(  BindFlag::ShaderResource|BindFlag::UnorderedAccess,
                                        BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, NativeFormat::R8_UNORM), "SSReflMask"), 
                nullptr)->AdoptUnderlying();
        UnorderedAccessView maskUnorderedAccess(maskTexture.get());
        ShaderResourceView maskShaderResource(maskTexture.get());

        auto reflectionsTexture = uploads.Transaction_Immediate(
                BuildRenderTargetDesc(  BindFlag::ShaderResource|BindFlag::UnorderedAccess|BindFlag::RenderTarget,
                                        BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, NativeFormat::R16G16B16A16_FLOAT), "SSRefl"), 
                nullptr)->AdoptUnderlying();
        UnorderedAccessView reflectionsUnorderedAccess(reflectionsTexture.get());
        RenderTargetView reflectionsTarget(reflectionsTexture.get());
        ShaderResourceView reflectionsShaderResource(reflectionsTexture.get());

        auto downsampledNormals = uploads.Transaction_Immediate(
                BuildRenderTargetDesc(  BindFlag::ShaderResource|BindFlag::RenderTarget,
                                        BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, NativeFormat::R16G16B16A16_FLOAT), "SSLowNorms"), //R11G11B10_FLOAT)), 
                nullptr)->AdoptUnderlying();
        RenderTargetView downsampledNormalsTarget(downsampledNormals.get());
        ShaderResourceView downsampledNormalsShaderResource(downsampledNormals.get());

        auto downsampledDepth = uploads.Transaction_Immediate(
                BuildRenderTargetDesc(  BindFlag::ShaderResource|BindFlag::RenderTarget,
                                        BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, NativeFormat::R32_FLOAT), "SSLowDepths"), // NativeFormat::R16_UNORM)), 
                nullptr)->AdoptUnderlying();
        RenderTargetView downsampledDepthTarget(downsampledDepth.get());
        ShaderResourceView downsampledDepthShaderResource(downsampledDepth.get());

            ////////////

                //      Build an irregular pattern of sample points.
                //      Start with grid points and jitter with random
                //      offsets.
        const unsigned samplesPerBlock = 64;
        const unsigned blockDimension = 64;
        struct SamplingPattern
        {
            unsigned samplePositions[samplesPerBlock][4];
            uint8 closestSamples[blockDimension][blockDimension][4];
            uint8 closestSamples2[blockDimension][blockDimension][4];
        };

        SamplingPattern samplingPattern;

        for (unsigned c=0; c<samplesPerBlock; ++c) {
            const int baseX = (c%8) * 9 + 1;
            const int baseY = (c/8) * 9 + 1;
            const int jitterRadius = desc._maskJitterRadius;     // note -- jitter radius larger than 4 can push the sample off the edge

            int offsetX = 0, offsetY = 0;
            if (jitterRadius > 0) {
                offsetX = rand() % ((2*jitterRadius)+1) - jitterRadius;
                offsetY = rand() % ((2*jitterRadius)+1) - jitterRadius;
            }
            
            samplingPattern.samplePositions[c][0] = std::min(std::max(baseX + offsetX, 0), 64-1);
            samplingPattern.samplePositions[c][1] = std::min(std::max(baseY + offsetY, 0), 64-1);
            samplingPattern.samplePositions[c][2] = 0;
            samplingPattern.samplePositions[c][3] = 0;
        }
    
        for (unsigned y=0; y<blockDimension; ++y) {
            for (unsigned x=0; x<blockDimension; ++x) {

                    //  Find the 4 closest samples
                const unsigned samplesToFind = 8;
                float closestDistanceSq[samplesToFind]     = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
                signed closestSampleIndex[samplesToFind]   = { -1, -1, -1, -1, -1, -1, -1, -1 };
                unsigned writtenSamples = 0;
                for (unsigned s=0; s<samplesPerBlock; ++s) {
                    float distanceSq = 
                          (float(samplingPattern.samplePositions[s][0]) - float(x)) * (float(samplingPattern.samplePositions[s][0]) - float(x))
                        + (float(samplingPattern.samplePositions[s][1]) - float(y)) * (float(samplingPattern.samplePositions[s][1]) - float(y))
                        ;

                    auto i = std::lower_bound(closestDistanceSq, &closestDistanceSq[writtenSamples], distanceSq);
                    if (i == &closestDistanceSq[writtenSamples]) {
                        if (writtenSamples < samplesToFind) {
                            closestDistanceSq[writtenSamples] = distanceSq;
                            closestSampleIndex[writtenSamples] = s;
                            ++writtenSamples;
                        }
                    } else {
                        auto insertIndex = std::distance(closestDistanceSq, i);
                        if (writtenSamples < samplesToFind) {
                            std::move(&closestDistanceSq[insertIndex], &closestDistanceSq[writtenSamples], &closestDistanceSq[insertIndex+1]);
                            std::move(&closestSampleIndex[insertIndex], &closestSampleIndex[writtenSamples], &closestSampleIndex[insertIndex+1]);
                            closestDistanceSq[insertIndex] = distanceSq;
                            closestSampleIndex[insertIndex] = s;
                            ++writtenSamples;
                        } else {
                            std::move(&closestDistanceSq[insertIndex], &closestDistanceSq[samplesToFind-1], &closestDistanceSq[insertIndex+1]);
                            std::move(&closestSampleIndex[insertIndex], &closestSampleIndex[samplesToFind-1], &closestSampleIndex[insertIndex+1]);
                            closestDistanceSq[insertIndex] = distanceSq;
                            closestSampleIndex[insertIndex] = s;
                        }
                    }
                }

                for (unsigned c=0; c<4; ++c) {
                    samplingPattern.closestSamples[y][x][c] = uint8(closestSampleIndex[c]);
                }
                for (unsigned c=0; c<4; ++c) {
                    samplingPattern.closestSamples2[y][x][c] = uint8(closestSampleIndex[4+c]);
                }

            }
        }

        ConstantBuffer samplingPatternConstants(&samplingPattern, sizeof(samplingPattern));

            ////////////
        auto* buildMask = &Assets::GetAssetDep<ComputeShader>(
            "game/xleres/screenspacerefl/buildmask.csh:BuildMask:cs_*");

        char definesBuffer[256];
        sprintf_s(definesBuffer, dimof(definesBuffer), "%sDOWNSAMPLE_SCALE=%i;INTERPOLATE_SAMPLES=%i", 
            desc._useMsaaSamplers?"MSAA_SAMPLERS=1;":"", desc._downsampleScale, int(desc._interpolateSamples));

        auto* buildReflections = &Assets::GetAssetDep<ComputeShader>(
            "game/xleres/screenspacerefl/buildreflection.csh:BuildReflection:cs_*",
            definesBuffer);
    
        auto* downsampleTargets = &Assets::GetAssetDep<ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*",
            "game/xleres/screenspacerefl/DownsampleStep.psh:main:ps_*",
            definesBuffer);

        auto* horizontalBlur = &Assets::GetAssetDep<ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*",
            "game/xleres/screenspacerefl/BlurStep.psh:HorizontalBlur:ps_*",
            definesBuffer);

        auto* verticalBlur = &Assets::GetAssetDep<ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*",
            "game/xleres/screenspacerefl/BlurStep.psh:VerticalBlur:ps_*",
            definesBuffer);

            ////////////
        auto validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(validationCallback, &buildMask->GetDependencyValidation());
        Assets::RegisterAssetDependency(validationCallback, &buildReflections->GetDependencyValidation());
        Assets::RegisterAssetDependency(validationCallback, &downsampleTargets->GetDependencyValidation());
        Assets::RegisterAssetDependency(validationCallback, &horizontalBlur->GetDependencyValidation());
        Assets::RegisterAssetDependency(validationCallback, &verticalBlur->GetDependencyValidation());

        _maskTexture = std::move(maskTexture);
        _maskUnorderedAccess = std::move(maskUnorderedAccess);
        _maskShaderResource = std::move(maskShaderResource);

        _reflectionsTexture = std::move(reflectionsTexture);
        _reflectionsUnorderedAccess = std::move(reflectionsUnorderedAccess);
        _reflectionsShaderResource = std::move(reflectionsShaderResource);
        _reflectionsTarget = std::move(reflectionsTarget);

        _downsampledNormals = std::move(downsampledNormals);
        _downsampledNormalsTarget = std::move(downsampledNormalsTarget);
        _downsampledNormalsShaderResource = std::move(downsampledNormalsShaderResource);

        _downsampledDepth = std::move(downsampledDepth);
        _downsampledDepthTarget = std::move(downsampledDepthTarget);
        _downsampledDepthShaderResource = std::move(downsampledDepthShaderResource);

        _buildMask = std::move(buildMask);
        _buildReflections = std::move(buildReflections);
        _downsampleTargets = std::move(downsampleTargets);
        _horizontalBlur = std::move(horizontalBlur);
        _verticalBlur = std::move(verticalBlur);
        _samplingPatternConstants = std::move(samplingPatternConstants);
        _validationCallback = std::move(validationCallback);
    }

    ScreenSpaceReflectionsResources::~ScreenSpaceReflectionsResources() {}

        ////////////////////////////////

    static void ScreenSpaceReflections_DrawDebugging(   RenderCore::Metal::DeviceContext* context, 
                                                        LightingParserContext& parserContext,
                                                        ScreenSpaceReflectionsResources& resources,
                                                        RenderCore::Metal::ShaderResourceView* gbufferDiffuse,
                                                        RenderCore::Metal::ShaderResourceView* gbufferNormals,
                                                        RenderCore::Metal::ShaderResourceView* gbufferParam,
                                                        RenderCore::Metal::ShaderResourceView* depthsSRV);

    ScreenSpaceReflectionsResources::Desc GetConfig(unsigned width, unsigned height, bool useMsaaSamplers)
    {
        unsigned reflScale = Tweakable("ReflectionScale", 2);
        unsigned reflWidth = width / reflScale;
        unsigned reflHeight = height / reflScale;
        unsigned reflMaskJitterRadius = Tweakable("ReflectionMaskJitterRadius", 0);
        bool interpolateSamples = Tweakable("ReflectionInterpolateSamples", true);
        return ScreenSpaceReflectionsResources::Desc(
                reflWidth, reflHeight, reflScale, 
                useMsaaSamplers, reflMaskJitterRadius, interpolateSamples);
    }

    RenderCore::Metal::ShaderResourceView
        ScreenSpaceReflections_BuildTextures(   RenderCore::Metal::DeviceContext* context, 
                                                LightingParserContext& parserContext,
                                                unsigned width, unsigned height, bool useMsaaSamplers, 
                                                RenderCore::Metal::ShaderResourceView& gbufferDiffuse,
                                                RenderCore::Metal::ShaderResourceView& gbufferNormals,
                                                RenderCore::Metal::ShaderResourceView& gbufferParam,
                                                RenderCore::Metal::ShaderResourceView& depthsSRV)
    {
            //
            //      Build textures and resources related to screen space textures
            //
        using namespace RenderCore;
        auto cfg = GetConfig(width, height, useMsaaSamplers);
        auto& res = FindCachedBoxDep<ScreenSpaceReflectionsResources>(cfg);

        SavedTargets oldTargets(context);
        SavedBlendAndRasterizerState oldBlendAndRasterizer(context);

        context->Bind(CommonResources()._blendOpaque);
        context->Bind(CommonResources()._cullDisable);
        ViewportDesc newViewport(0, 0, float(cfg._width), float(cfg._height), 0.f, 1.f);
        context->Bind(newViewport);

            //  
            //      Downsample the normal & depth textures 
            //
        context->Bind(MakeResourceList(res._downsampledDepthTarget, res._downsampledNormalsTarget), nullptr);
        context->BindPS(MakeResourceList(1, gbufferNormals));
        context->BindPS(MakeResourceList(3, depthsSRV));
        context->BindPS(MakeResourceList(parserContext.GetGlobalTransformCB()));
        context->Bind(*res._downsampleTargets);
        SetupVertexGeneratorShader(context);
        context->Draw(4);

        auto& projDesc = parserContext.GetProjectionDesc();
        Float3 projScale; float projZOffset;
        {
                // this is a 4-parameter minimal projection transform
                // see "RenderCore::Assets::PerspectiveProjection" for more detail
            const float n = projDesc._nearClip;
            const float f = projDesc._farClip;
            const float h = n * XlTan(.5f * projDesc._verticalFov);
            const float w = h * projDesc._aspectRatio;
            const float l = -w, r = w;
            const float t = h, b = -h;
            projScale[0] = (2.f * n) / (r-l);
            projScale[1] = (2.f * n) / (t-b);
            projScale[2] = -(f) / (f-n);
            projZOffset = -(f*n) / (f-n);
        }

        struct ViewProjectionParameters
        {
            Float4x4    _worldToView;
            Float3      _projScale;
            float       _projZOffset;
        } viewProjParam = {
            InvertOrthonormalTransform(projDesc._cameraToWorld),
            projScale, projZOffset
        };

            //
            //      Build the mask texture by sampling the downsampled textures
            //
        context->Bind(ResourceList<RenderTargetView, 0>(), nullptr);
        context->BindCS(MakeResourceList(gbufferDiffuse, res._downsampledNormalsShaderResource, res._downsampledDepthShaderResource));
        context->BindCS(MakeResourceList(4, Assets::GetAssetDep<RenderCore::Metal::DeferredShaderResource>("game/xleres/DefaultResources/balanced_noise.dds").GetShaderResource()));
        context->BindCS(MakeResourceList(res._maskUnorderedAccess));
        context->BindCS(MakeResourceList(parserContext.GetGlobalTransformCB(), Metal::ConstantBuffer(&viewProjParam, sizeof(viewProjParam)), res._samplingPatternConstants, parserContext.GetGlobalStateCB()));
        context->BindCS(MakeResourceList(   Metal::SamplerState(Metal::FilterMode::Trilinear, Metal::AddressMode::Wrap, Metal::AddressMode::Wrap, Metal::AddressMode::Wrap),
                                            Metal::SamplerState(Metal::FilterMode::Trilinear, Metal::AddressMode::Clamp, Metal::AddressMode::Clamp, Metal::AddressMode::Clamp)));
        context->Bind(*res._buildMask);
        context->Dispatch((cfg._width + (64-1))/64, (cfg._height + (64-1))/64);

            //
            //      Now write the reflections texture
            //
        context->BindCS(MakeResourceList(res._reflectionsUnorderedAccess));
        context->BindCS(MakeResourceList(3, res._maskShaderResource));
        context->Bind(*res._buildReflections);
        context->Dispatch((cfg._width + (16-1))/16, (cfg._height + (16-1))/16);

        context->UnbindCS<UnorderedAccessView>(0, 1);
        context->UnbindCS<ShaderResourceView>(0, 4);

            //
            //      Blur the result in 2 steps
            //          (use the normals target as a temporary buffer here)
            //
        if (Tweakable("ReflectionDoBlur", false)) {
            float filteringWeights[8];
            XlSetMemory(filteringWeights, 0, sizeof(filteringWeights));
            static float standardDeviation = 1.2f; // 0.809171316279f; // 1.6f;
            BuildGaussianFilteringWeights(filteringWeights, standardDeviation, 7);
            context->BindPS(MakeResourceList(Metal::ConstantBuffer(filteringWeights, sizeof(filteringWeights))));

            context->Bind(MakeResourceList(res._downsampledNormalsTarget), nullptr);
            context->BindPS(MakeResourceList(0, res._reflectionsShaderResource));
            context->Bind(*res._horizontalBlur);
            context->Draw(4);
            context->UnbindPS<ShaderResourceView>(0, 1);

            context->Bind(MakeResourceList(res._reflectionsTarget), nullptr);
            context->BindPS(MakeResourceList(0, res._downsampledNormalsShaderResource));
            context->Bind(*res._verticalBlur);
            context->Draw(4);
        }

        oldTargets.ResetToOldTargets(context);
        oldBlendAndRasterizer.ResetToOldStates(context);

        if (Tweakable("ScreenspaceReflectionDebugging", false)) {
            parserContext._pendingOverlays.push_back(
                std::bind(  &ScreenSpaceReflections_DrawDebugging, 
                            std::placeholders::_1, std::placeholders::_2, res, &gbufferDiffuse, &gbufferNormals, &gbufferNormals, &depthsSRV));
        }

        return res._reflectionsShaderResource;
    }

    static Int2 GetCursorPos()
    {
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        ScreenToClient((HWND)::GetActiveWindow(), &cursorPos);
        return Int2(cursorPos.x, cursorPos.y);
    }

    static void ScreenSpaceReflections_DrawDebugging(   RenderCore::Metal::DeviceContext* context, 
                                                        LightingParserContext& parserContext,
                                                        ScreenSpaceReflectionsResources& resources,
                                                        RenderCore::Metal::ShaderResourceView* gbufferDiffuse,
                                                        RenderCore::Metal::ShaderResourceView* gbufferNormals,
                                                        RenderCore::Metal::ShaderResourceView* gbufferParam,
                                                        RenderCore::Metal::ShaderResourceView* depthsSRV)
    {
        TRY {
            char definesBuffer[256];
            sprintf_s(definesBuffer, dimof(definesBuffer), "%sDOWNSAMPLE_SCALE=%i;INTERPOLATE_SAMPLES=%i", 
                resources._desc._useMsaaSamplers?"MSAA_SAMPLERS=1;":"", resources._desc._downsampleScale, int(resources._desc._interpolateSamples));
            auto& debuggingShader = Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2D.vsh:fullscreen_viewfrustumvector:vs_*", 
                "game/xleres/screenspacerefl/debugging.psh:main:ps_*",
                definesBuffer);
            context->Bind(debuggingShader);
            context->Bind(CommonResources()._blendStraightAlpha);

            context->BindPS(MakeResourceList(5, resources._maskShaderResource, resources._downsampledNormalsShaderResource));
            context->BindPS(MakeResourceList(10, resources._downsampledDepthShaderResource, resources._reflectionsShaderResource));
            auto skyTexture = parserContext.GetSceneParser()->GetGlobalLightingDesc()._skyTexture;
            if (skyTexture) {
                SkyTexture_BindPS(context, parserContext, skyTexture, 7);
            }

                // todo -- we have to bind the gbuffer here!
            context->BindPS(MakeResourceList(*gbufferDiffuse, *gbufferNormals, *gbufferParam, *depthsSRV));

            ViewportDesc mainViewportDesc(*context);
                            
            auto cursorPos = GetCursorPos();
            unsigned globalConstants[4] = { unsigned(mainViewportDesc.Width), unsigned(mainViewportDesc.Height), cursorPos[0], cursorPos[1] };
            Metal::ConstantBuffer globalConstantsBuffer(globalConstants, sizeof(globalConstants));
            // context->BindPS(MakeResourceList(globalConstantsBuffer, res._samplingPatternConstants));
            Metal::BoundUniforms boundUniforms(debuggingShader);
            boundUniforms.BindConstantBuffer(Hash64("BasicGlobals"), 0, 1);
            boundUniforms.BindConstantBuffer(Hash64("SamplingPattern"), 1, 1);
            TechniqueContext::BindGlobalUniforms(boundUniforms);
            const Metal::ConstantBuffer* prebuiltBuffers[] = { &globalConstantsBuffer, &resources._samplingPatternConstants };
            boundUniforms.Apply(*context, 
                parserContext.GetGlobalUniformsStream(),
                UniformsStream(nullptr, prebuiltBuffers, dimof(prebuiltBuffers)));

            SetupVertexGeneratorShader(context);
            context->Draw(4);

            context->UnbindPS<ShaderResourceView>(0, 9);
        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH_END
    }
}


