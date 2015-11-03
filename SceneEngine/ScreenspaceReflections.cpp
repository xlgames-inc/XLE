// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS         // suppress warning related to std::move (algorithm.h version)

#include "ScreenspaceReflections.h"
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "SceneParser.h"
#include "Sky.h"
#include "LightDesc.h"
#include "GestaltResource.h"

#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Assets/DeferredShaderResource.h"
#include "../RenderCore/Metal/RenderTargetView.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../Math/Transformations.h"
#include "../Utility/StringFormat.h"

#include "../ConsoleRig/Console.h"

namespace SceneEngine
{
    using namespace RenderCore;

    class ScreenSpaceReflectionsResources
    {
    public:
        class Desc
        {
        public:
            unsigned    _width, _height;
            unsigned    _downsampleScale;
            bool        _useMsaaSamplers;
            unsigned    _maskJitterRadius;
            bool        _interpolateSamples;
            Desc(   unsigned width, unsigned height, unsigned downsampleScale, bool useMsaaSamplers, 
                    unsigned maskJitterRadius, bool interpolateSamples) 
            : _useMsaaSamplers(useMsaaSamplers), _width(width), _height(height)
            , _downsampleScale(downsampleScale), _maskJitterRadius(maskJitterRadius)
            , _interpolateSamples(interpolateSamples) {}
        };

        using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;

        // ResLocator                      _maskTexture;
        // Metal::UnorderedAccessView      _maskUnorderedAccess;
        // Metal::ShaderResourceView       _maskShaderResource;
        GestaltTypes::UAVSRV            _mask;
        const Metal::ComputeShader*     _buildMask;

        // ResLocator                      _reflectionsTexture;
        // Metal::RenderTargetView         _reflectionsTarget;
        // Metal::UnorderedAccessView      _reflectionsUnorderedAccess;
        // Metal::ShaderResourceView       _reflectionsShaderResource;
        GestaltTypes::RTVUAVSRV         _reflections;
		const Metal::ComputeShader*     _buildReflections;

        // ResLocator                      _downsampledNormals;
        // ResLocator                      _downsampledDepth;
        // Metal::RenderTargetView         _downsampledNormalsTarget;
        // Metal::RenderTargetView         _downsampledDepthTarget;
        // Metal::ShaderResourceView       _downsampledNormalsShaderResource;
        // Metal::ShaderResourceView       _downsampledDepthShaderResource;
        GestaltTypes::RTVSRV            _downsampledNormals;
        GestaltTypes::RTVSRV            _downsampledDepth;

        const Metal::ShaderProgram*     _downsampleTargets;
        const Metal::ShaderProgram*     _horizontalBlur;
        const Metal::ShaderProgram*     _verticalBlur;

        Metal::ConstantBuffer           _samplingPatternConstants;

        Desc _desc;

        ScreenSpaceReflectionsResources(const Desc& desc);
        ~ScreenSpaceReflectionsResources();

        ScreenSpaceReflectionsResources(const ScreenSpaceReflectionsResources&) = delete;
        ScreenSpaceReflectionsResources& operator=(const ScreenSpaceReflectionsResources&) = delete;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const   { return _validationCallback; }
    private:
        std::shared_ptr<::Assets::DependencyValidation>  _validationCallback;
    };

    ScreenSpaceReflectionsResources::ScreenSpaceReflectionsResources(const Desc& desc)
    : _desc(desc)
    {
        using namespace BufferUploads;
    
            ////////////
        // auto maskTexture = uploads.Transaction_Immediate(
        //     CreateDesc(  
        //         BindFlag::ShaderResource|BindFlag::UnorderedAccess,
        //         0, GPUAccess::Read|GPUAccess::Write,
        //         TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R8_UNORM),
        //         "SSReflMask"));
        // Metal::UnorderedAccessView maskUnorderedAccess(maskTexture->GetUnderlying());
        // Metal::ShaderResourceView maskShaderResource(maskTexture->GetUnderlying());
        
        // auto reflectionsTexture = uploads.Transaction_Immediate(
        //     CreateDesc(
        //         BindFlag::ShaderResource|BindFlag::UnorderedAccess|BindFlag::RenderTarget,
        //         0, GPUAccess::Read|GPUAccess::Write,
        //         TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R16G16B16A16_FLOAT), 
        //         "SSRefl"));
        // Metal::UnorderedAccessView reflectionsUnorderedAccess(reflectionsTexture->GetUnderlying());
        // Metal::RenderTargetView reflectionsTarget(reflectionsTexture->GetUnderlying());
        // Metal::ShaderResourceView reflectionsShaderResource(reflectionsTexture->GetUnderlying());
        // 
        // auto downsampledNormals = uploads.Transaction_Immediate(
        //     CreateDesc(  
        //         BindFlag::ShaderResource|BindFlag::RenderTarget,
        //         0, GPUAccess::Read|GPUAccess::Write,
        //         TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R16G16B16A16_FLOAT), //R11G11B10_FLOAT)), 
        //         "SSLowNorms"));
        // Metal::RenderTargetView downsampledNormalsTarget(downsampledNormals->GetUnderlying());
        // Metal::ShaderResourceView downsampledNormalsShaderResource(downsampledNormals->GetUnderlying());
        // 
        // auto downsampledDepth = uploads.Transaction_Immediate(
        //     CreateDesc( 
        //         BindFlag::ShaderResource|BindFlag::RenderTarget,
        //         0, GPUAccess::Read|GPUAccess::Write,
        //         TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R32_FLOAT), // NativeFormat::R16_UNORM)), 
        //         "SSLowDepths"));
        // Metal::RenderTargetView downsampledDepthTarget(downsampledDepth->GetUnderlying());
        // Metal::ShaderResourceView downsampledDepthShaderResource(downsampledDepth->GetUnderlying());

        _mask = GestaltTypes::UAVSRV(
            TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R8_UNORM),
            "SSReflMask");

        _reflections = GestaltTypes::RTVUAVSRV(
            TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R16G16B16A16_FLOAT), 
            "SSRefl");

        _downsampledNormals = GestaltTypes::RTVSRV(
            TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R16G16B16A16_FLOAT), //R11G11B10_FLOAT), 
            "SSLowNorms");

        _downsampledDepth = GestaltTypes::RTVSRV(
            TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R32_FLOAT), // NativeFormat::R16_UNORM), 
            "SSLowDepths");

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

		auto samplingPattern = std::make_unique<SamplingPattern>();

        for (unsigned c=0; c<samplesPerBlock; ++c) {
            const int baseX = (c%8) * 9 + 1;
            const int baseY = (c/8) * 9 + 1;
            const int jitterRadius = desc._maskJitterRadius;     // note -- jitter radius larger than 4 can push the sample off the edge

            int offsetX = 0, offsetY = 0;
            if (jitterRadius > 0) {
                offsetX = rand() % ((2*jitterRadius)+1) - jitterRadius;
                offsetY = rand() % ((2*jitterRadius)+1) - jitterRadius;
            }
            
            samplingPattern->samplePositions[c][0] = std::min(std::max(baseX + offsetX, 0), 64-1);
            samplingPattern->samplePositions[c][1] = std::min(std::max(baseY + offsetY, 0), 64-1);
            samplingPattern->samplePositions[c][2] = 0;
            samplingPattern->samplePositions[c][3] = 0;
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
                          (float(samplingPattern->samplePositions[s][0]) - float(x)) * (float(samplingPattern->samplePositions[s][0]) - float(x))
                        + (float(samplingPattern->samplePositions[s][1]) - float(y)) * (float(samplingPattern->samplePositions[s][1]) - float(y))
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
                    samplingPattern->closestSamples[y][x][c] = uint8(closestSampleIndex[c]);
                }
                for (unsigned c=0; c<4; ++c) {
                    samplingPattern->closestSamples2[y][x][c] = uint8(closestSampleIndex[4+c]);
                }

            }
        }

		Metal::ConstantBuffer samplingPatternConstants(samplingPattern.get(), sizeof(SamplingPattern));

            ////////////
        _buildMask = &::Assets::GetAssetDep<Metal::ComputeShader>(
            "game/xleres/screenspacerefl/buildmask.csh:BuildMask:cs_*");

        StringMeld<256> definesBuffer;
        definesBuffer 
            << (desc._useMsaaSamplers?"MSAA_SAMPLERS=1;":"") 
            << "DOWNSAMPLE_SCALE=" << desc._downsampleScale 
            << ";INTERPOLATE_SAMPLES=" << int(desc._interpolateSamples);

        _buildReflections = &::Assets::GetAssetDep<Metal::ComputeShader>(
            "game/xleres/screenspacerefl/buildreflection.csh:BuildReflection:cs_*",
            definesBuffer.get());
    
        _downsampleTargets = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*",
            "game/xleres/screenspacerefl/DownsampleStep.psh:main:ps_*",
            definesBuffer.get());

        _horizontalBlur = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*",
            "game/xleres/screenspacerefl/BlurStep.psh:HorizontalBlur:ps_*",
            definesBuffer.get());

        _verticalBlur = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*",
            "game/xleres/screenspacerefl/BlurStep.psh:VerticalBlur:ps_*",
            definesBuffer.get());

            ////////////
        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_validationCallback, _buildMask->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, _buildReflections->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, _downsampleTargets->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, _horizontalBlur->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, _verticalBlur->GetDependencyValidation());

        // _reflectionsTexture = std::move(reflectionsTexture);
        // _reflectionsUnorderedAccess = std::move(reflectionsUnorderedAccess);
        // _reflectionsShaderResource = std::move(reflectionsShaderResource);
        // _reflectionsTarget = std::move(reflectionsTarget);
        // 
        // _downsampledNormals = std::move(downsampledNormals);
        // _downsampledNormalsTarget = std::move(downsampledNormalsTarget);
        // _downsampledNormalsShaderResource = std::move(downsampledNormalsShaderResource);
        // 
        // _downsampledDepth = std::move(downsampledDepth);
        // _downsampledDepthTarget = std::move(downsampledDepthTarget);
        // _downsampledDepthShaderResource = std::move(downsampledDepthShaderResource);

        _samplingPatternConstants = std::move(samplingPatternConstants);
    }

    ScreenSpaceReflectionsResources::~ScreenSpaceReflectionsResources() {}

        ////////////////////////////////

    static void ScreenSpaceReflections_DrawDebugging(   Metal::DeviceContext& context, 
                                                        LightingParserContext& parserContext,
                                                        ScreenSpaceReflectionsResources& resources,
                                                        Metal::ShaderResourceView* gbufferDiffuse,
                                                        Metal::ShaderResourceView* gbufferNormals,
                                                        Metal::ShaderResourceView* gbufferParam,
                                                        Metal::ShaderResourceView* depthsSRV);

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

    Metal::ShaderResourceView
        ScreenSpaceReflections_BuildTextures(   Metal::DeviceContext* context, 
                                                LightingParserContext& parserContext,
                                                unsigned width, unsigned height, bool useMsaaSamplers, 
                                                Metal::ShaderResourceView& gbufferDiffuse,
                                                Metal::ShaderResourceView& gbufferNormals,
                                                Metal::ShaderResourceView& gbufferParam,
                                                Metal::ShaderResourceView& depthsSRV)
    {
            //
            //      Build textures and resources related to screen space textures
            //
        using namespace RenderCore;
        auto cfg = GetConfig(width, height, useMsaaSamplers);
        auto& res = Techniques::FindCachedBoxDep<ScreenSpaceReflectionsResources>(cfg);

        ProtectState protectState(
            *context,
              ProtectState::States::RenderTargets | ProtectState::States::Viewports
            | ProtectState::States::BlendState | ProtectState::States::RasterizerState);

        context->Bind(Techniques::CommonResources()._blendOpaque);
        context->Bind(Techniques::CommonResources()._cullDisable);
        Metal::ViewportDesc newViewport(0, 0, float(cfg._width), float(cfg._height), 0.f, 1.f);
        context->Bind(newViewport);

            //  
            //      Downsample the normal & depth textures 
            //
        context->Bind(MakeResourceList(res._downsampledDepth.RTV(), res._downsampledNormals.RTV()), nullptr);
        context->BindPS(MakeResourceList(1, gbufferNormals));
        context->BindPS(MakeResourceList(3, depthsSRV));
        context->BindPS(MakeResourceList(parserContext.GetGlobalTransformCB()));
        context->Bind(*res._downsampleTargets);
        SetupVertexGeneratorShader(*context);
        context->Draw(4);

        auto& projDesc = parserContext.GetProjectionDesc();
        Float3 projScale; float projZOffset;
        {
                // this is a 4-parameter minimal projection transform
                // see "PerspectiveProjection" for more detail
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
        context->Bind(ResourceList<Metal::RenderTargetView, 0>(), nullptr);
        context->BindCS(MakeResourceList(gbufferDiffuse, res._downsampledNormals.SRV(), res._downsampledDepth.SRV()));
        context->BindCS(MakeResourceList(4, ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/DefaultResources/balanced_noise.dds:LT").GetShaderResource()));
        context->BindCS(MakeResourceList(res._mask.UAV()));
        context->BindCS(MakeResourceList(parserContext.GetGlobalTransformCB(), Metal::ConstantBuffer(&viewProjParam, sizeof(viewProjParam)), res._samplingPatternConstants, parserContext.GetGlobalStateCB()));
        context->BindCS(MakeResourceList(   Metal::SamplerState(Metal::FilterMode::Trilinear, Metal::AddressMode::Wrap, Metal::AddressMode::Wrap, Metal::AddressMode::Wrap),
                                            Metal::SamplerState(Metal::FilterMode::Trilinear, Metal::AddressMode::Clamp, Metal::AddressMode::Clamp, Metal::AddressMode::Clamp)));
        context->Bind(*res._buildMask);
        context->Dispatch((cfg._width + (64-1))/64, (cfg._height + (64-1))/64);

            //
            //      Now write the reflections texture
            //
        context->BindCS(MakeResourceList(res._reflections.UAV()));
        context->BindCS(MakeResourceList(3, res._mask.SRV()));
        context->Bind(*res._buildReflections);
        context->Dispatch((cfg._width + (16-1))/16, (cfg._height + (16-1))/16);

        context->UnbindCS<Metal::UnorderedAccessView>(0, 1);
        context->UnbindCS<Metal::ShaderResourceView>(0, 4);

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

            context->Bind(MakeResourceList(res._downsampledNormals.RTV()), nullptr);
            context->BindPS(MakeResourceList(0, res._reflections.SRV()));
            context->Bind(*res._horizontalBlur);
            context->Draw(4);
            context->UnbindPS<Metal::ShaderResourceView>(0, 1);

            context->Bind(MakeResourceList(res._reflections.RTV()), nullptr);
            context->BindPS(MakeResourceList(0, res._downsampledNormals.SRV()));
            context->Bind(*res._verticalBlur);
            context->Draw(4);
        }

        if (Tweakable("ScreenspaceReflectionDebugging", false)) {
            parserContext._pendingOverlays.push_back(
                std::bind(  &ScreenSpaceReflections_DrawDebugging, 
                            std::placeholders::_1, std::placeholders::_2, std::ref(res), &gbufferDiffuse, &gbufferNormals, &gbufferNormals, &depthsSRV));
        }

        return res._reflections.SRV();
    }

    static void ScreenSpaceReflections_DrawDebugging(   Metal::DeviceContext& context, 
                                                        LightingParserContext& parserContext,
                                                        ScreenSpaceReflectionsResources& resources,
                                                        Metal::ShaderResourceView* gbufferDiffuse,
                                                        Metal::ShaderResourceView* gbufferNormals,
                                                        Metal::ShaderResourceView* gbufferParam,
                                                        Metal::ShaderResourceView* depthsSRV)
    {
        StringMeld<256> definesBuffer;
        definesBuffer 
            << (resources._desc._useMsaaSamplers?"MSAA_SAMPLERS=1;":"") 
            << "DOWNSAMPLE_SCALE=" << resources._desc._downsampleScale 
            << ";INTERPOLATE_SAMPLES=" << int(resources._desc._interpolateSamples);
        auto& debuggingShader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen_viewfrustumvector:vs_*", 
            "game/xleres/screenspacerefl/debugging.psh:main:ps_*",
            definesBuffer.get());
        context.Bind(debuggingShader);
        context.Bind(Techniques::CommonResources()._blendStraightAlpha);

        context.BindPS(MakeResourceList(5, resources._mask.SRV(), resources._downsampledNormals.SRV()));
        context.BindPS(MakeResourceList(10, resources._downsampledDepth.SRV(), resources._reflections.SRV()));
        auto skyTexture = parserContext.GetSceneParser()->GetGlobalLightingDesc()._skyTexture;
        if (skyTexture[0]) {
            SkyTexture_BindPS(&context, parserContext, skyTexture, 7);
        }

            // todo -- we have to bind the gbuffer here!
        context.BindPS(MakeResourceList(*gbufferDiffuse, *gbufferNormals, *gbufferParam, *depthsSRV));

        Metal::ViewportDesc mainViewportDesc(context);
                            
        auto cursorPos = GetCursorPos();
        unsigned globalConstants[4] = { unsigned(mainViewportDesc.Width), unsigned(mainViewportDesc.Height), cursorPos[0], cursorPos[1] };
        Metal::ConstantBuffer globalConstantsBuffer(globalConstants, sizeof(globalConstants));
        // context->BindPS(MakeResourceList(globalConstantsBuffer, res._samplingPatternConstants));
        Metal::BoundUniforms boundUniforms(debuggingShader);
        boundUniforms.BindConstantBuffer(Hash64("BasicGlobals"), 0, 1);
        boundUniforms.BindConstantBuffer(Hash64("SamplingPattern"), 1, 1);
        Techniques::TechniqueContext::BindGlobalUniforms(boundUniforms);
        const Metal::ConstantBuffer* prebuiltBuffers[] = { &globalConstantsBuffer, &resources._samplingPatternConstants };
        boundUniforms.Apply(context, 
            parserContext.GetGlobalUniformsStream(),
            Metal::UniformsStream(nullptr, prebuiltBuffers, dimof(prebuiltBuffers)));

        SetupVertexGeneratorShader(context);
        context.Draw(4);

        context.UnbindPS<Metal::ShaderResourceView>(0, 9);
    }
}


