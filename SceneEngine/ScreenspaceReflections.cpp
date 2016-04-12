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
#include "../RenderCore/Metal/TextureView.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../BufferUploads/DataPacket.h"
#include "../Math/Transformations.h"
#include "../Utility/StringFormat.h"

#include "../ConsoleRig/Console.h"
#include <random>

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
            bool        _hasGBufferProperties;
            Desc(   unsigned width, unsigned height, unsigned downsampleScale, bool useMsaaSamplers, 
                    unsigned maskJitterRadius, bool interpolateSamples, bool hasGBufferProperties) 
            : _useMsaaSamplers(useMsaaSamplers), _width(width), _height(height)
            , _downsampleScale(downsampleScale), _maskJitterRadius(maskJitterRadius)
            , _interpolateSamples(interpolateSamples), _hasGBufferProperties(hasGBufferProperties) {}
        };

        using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;

        GestaltTypes::UAVSRV            _mask;
        const Metal::ComputeShader*     _buildMask;

        GestaltTypes::RTVUAVSRV         _reflections;
		const Metal::ComputeShader*     _buildReflections;

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

        const ::Assets::DepValPtr& GetDependencyValidation() const   { return _depVal; }
    private:
        ::Assets::DepValPtr  _depVal;
    };

    ScreenSpaceReflectionsResources::ScreenSpaceReflectionsResources(const Desc& desc)
    : _desc(desc)
    {
        using namespace BufferUploads;
    
            ////////////
        _mask = GestaltTypes::UAVSRV(
            TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R8G8_UNORM),
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

            // note -- it might be helpful for the pattern to be a but
            //      non-uniform. But probably a non-random pattern would
            //      be best (eg, using a variation of the hammersly pattern)
        std::mt19937 generator(std::random_device().operator()());
        for (unsigned c=0; c<samplesPerBlock; ++c) {
            const int baseX = (c%8) * 9 + 1;
            const int baseY = (c/8) * 9 + 1;
            const int jitterRadius = desc._maskJitterRadius;     // note -- jitter radius larger than 4 can push the sample off the edge
            
            int offsetX = 0, offsetY = 0;
            if (jitterRadius > 0) {
                std::uniform_int_distribution<> dist(-jitterRadius, jitterRadius);
                offsetX = dist(generator);
                offsetY = dist(generator);
            }
            
            samplingPattern->samplePositions[c][0] = std::min(std::max(baseX + offsetX, 0), 64-1);
            samplingPattern->samplePositions[c][1] = std::min(std::max(baseY + offsetY, 0), 64-1);
            samplingPattern->samplePositions[c][2] = 0;
            samplingPattern->samplePositions[c][3] = 0;

            // samplingPattern->samplePositions[c][0] = (c%8) * 8 + 4;
            // samplingPattern->samplePositions[c][1] = (c/8) * 8 + 4;
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

		_samplingPatternConstants = Metal::ConstantBuffer(samplingPattern.get(), sizeof(SamplingPattern));

        // using namespace BufferUploads;
        // auto pkt = CreateBasicPacket(sizeof(SamplingPattern), samplingPattern.get());
        // auto samplingPatternCS = GetBufferUploads().Transaction_Immediate(
        //     CreateDesc(
        //         BindFlag::StructuredBuffer|BindFlag::ShaderResource, 0, GPUAccess::Read, 
        //         LinearBufferDesc::Create(sizeof(SamplingPattern), sizeof(unsigned)),
        //         "SSRSampling"),
        //     pkt.get());
        // _samplingPatternConstantsCS = Metal::ShaderResourceView(samplingPatternCS->GetUnderlying());

            ////////////
        _buildMask = &::Assets::GetAssetDep<Metal::ComputeShader>(
            "game/xleres/screenspacerefl/buildmask.csh:BuildMask:cs_*");

        StringMeld<256> definesBuffer;
        definesBuffer 
            << (desc._useMsaaSamplers?"MSAA_SAMPLERS=1;":"") 
            << "DOWNSAMPLE_SCALE=" << desc._downsampleScale 
            << ";INTERPOLATE_SAMPLES=" << int(desc._interpolateSamples)
            << ";GBUFFER_TYPE=" << desc._hasGBufferProperties?1:2;
            ;

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
        _depVal = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_depVal, _buildMask->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_depVal, _buildReflections->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_depVal, _downsampleTargets->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_depVal, _horizontalBlur->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_depVal, _verticalBlur->GetDependencyValidation());
    }

    ScreenSpaceReflectionsResources::~ScreenSpaceReflectionsResources() {}

        ////////////////////////////////

    static void ScreenSpaceReflections_DrawDebugging(   Metal::DeviceContext& context, 
                                                        LightingParserContext& parserContext,
                                                        ScreenSpaceReflectionsResources& resources,
                                                        Metal::ShaderResourceView gbufferDiffuse,
                                                        Metal::ShaderResourceView gbufferNormals,
                                                        Metal::ShaderResourceView gbufferParam,
                                                        Metal::ShaderResourceView depthsSRV);

    ScreenSpaceReflectionsResources::Desc GetConfig(unsigned width, unsigned height, bool useMsaaSamplers, bool hasGBufferProperties)
    {
        unsigned reflScale = Tweakable("ReflectionScale", 2);
        unsigned reflWidth = width / reflScale;
        unsigned reflHeight = height / reflScale;
        unsigned reflMaskJitterRadius = Tweakable("ReflectionMaskJitterRadius", 2);
        bool interpolateSamples = Tweakable("ReflectionInterpolateSamples", true);
        return ScreenSpaceReflectionsResources::Desc(
                reflWidth, reflHeight, reflScale, 
                useMsaaSamplers, reflMaskJitterRadius, interpolateSamples,
                hasGBufferProperties);
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
        auto cfg = GetConfig(width, height, useMsaaSamplers, gbufferParam.IsGood());
        auto& res = Techniques::FindCachedBoxDep<ScreenSpaceReflectionsResources>(cfg);

        ProtectState protectState(
            *context,
              ProtectState::States::RenderTargets | ProtectState::States::Viewports
            | ProtectState::States::BlendState | ProtectState::States::RasterizerState);

        auto& commonResources = Techniques::CommonResources();
        context->Bind(commonResources._blendOpaque);
        context->Bind(commonResources._cullDisable);
        Metal::ViewportDesc newViewport(0, 0, float(cfg._width), float(cfg._height), 0.f, 1.f);
        context->Bind(newViewport);

            //  
            //      Downsample the normal & depth textures 
            //
        context->Bind(MakeResourceList(res._downsampledDepth.RTV(), res._downsampledNormals.RTV()), nullptr);
        context->BindPS(MakeResourceList(gbufferDiffuse, gbufferNormals, gbufferParam, depthsSRV));
        context->BindPS(MakeResourceList(parserContext.GetGlobalTransformCB()));
        context->Bind(*res._downsampleTargets);
        SetupVertexGeneratorShader(*context);
        context->Draw(4);

        auto& projDesc = parserContext.GetProjectionDesc();
        struct ViewProjectionParameters
        {
            Float4x4    _worldToView;
        } viewProjParam = {
            InvertOrthonormalTransform(projDesc._cameraToWorld)
        };

            //
            //      Build the mask texture by sampling the downsampled textures
            //
        context->Bind(ResourceList<Metal::RenderTargetView, 0>(), nullptr);
        context->BindCS(MakeResourceList(gbufferDiffuse, res._downsampledNormals.SRV(), res._downsampledDepth.SRV()));
        context->BindCS(MakeResourceList(res._mask.UAV()));
        context->BindCS(MakeResourceList(parserContext.GetGlobalTransformCB(), Metal::ConstantBuffer(&viewProjParam, sizeof(viewProjParam)), res._samplingPatternConstants, Metal::ConstantBuffer(), parserContext.GetGlobalStateCB()));
        context->BindCS(MakeResourceList(commonResources._linearWrapSampler, commonResources._linearClampSampler));
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
                            std::placeholders::_1, std::placeholders::_2, std::ref(res), gbufferDiffuse, gbufferNormals, gbufferParam, depthsSRV));
        }

        return res._reflections.SRV();
    }

    static void ScreenSpaceReflections_DrawDebugging(   Metal::DeviceContext& context, 
                                                        LightingParserContext& parserContext,
                                                        ScreenSpaceReflectionsResources& resources,
                                                        Metal::ShaderResourceView gbufferDiffuse,
                                                        Metal::ShaderResourceView gbufferNormals,
                                                        Metal::ShaderResourceView gbufferParam,
                                                        Metal::ShaderResourceView depthsSRV)
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
        SkyTextureParts(parserContext.GetSceneParser()->GetGlobalLightingDesc()).BindPS(context, 7);

            // todo -- we have to bind the gbuffer here!
        context.InvalidateCachedState();
        context.BindPS(MakeResourceList(gbufferDiffuse, gbufferNormals, gbufferParam, depthsSRV));

        Metal::ViewportDesc mainViewportDesc(context);
                            
        auto cursorPos = GetCursorPos();
        unsigned globalConstants[4] = { unsigned(mainViewportDesc.Width), unsigned(mainViewportDesc.Height), cursorPos[0], cursorPos[1] };
        Metal::ConstantBuffer globalConstantsBuffer(globalConstants, sizeof(globalConstants));

        auto& projDesc = parserContext.GetProjectionDesc();
        struct ViewProjectionParameters { Float4x4    _worldToView; }
            viewProjParam = { InvertOrthonormalTransform(projDesc._cameraToWorld) };
        Metal::ConstantBuffer viewProjectionParametersBuffer(&viewProjParam, sizeof(viewProjParam));

        Metal::BoundUniforms boundUniforms(debuggingShader);
        boundUniforms.BindConstantBuffer(Hash64("BasicGlobals"), 0, 1);
        boundUniforms.BindConstantBuffer(Hash64("SamplingPattern"), 1, 1);
        boundUniforms.BindConstantBuffer(Hash64("ViewProjectionParameters"), 2, 1);
        Techniques::TechniqueContext::BindGlobalUniforms(boundUniforms);
        const Metal::ConstantBuffer* prebuiltBuffers[] = { &globalConstantsBuffer, &resources._samplingPatternConstants, &viewProjectionParametersBuffer };
        boundUniforms.Apply(context, 
            parserContext.GetGlobalUniformsStream(),
            Metal::UniformsStream(nullptr, prebuiltBuffers, dimof(prebuiltBuffers)));

        SetupVertexGeneratorShader(context);
        context.Draw(4);

        context.UnbindPS<Metal::ShaderResourceView>(0, 9);
    }
}


