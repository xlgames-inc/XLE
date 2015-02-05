// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "VolumetricFog.h"
#include "ResourceBox.h"
#include "SceneEngineUtility.h"
#include "LightDesc.h"
#include "Noise.h"
#include "CommonResources.h"
#include "LightInternal.h"

#include "../RenderCore/Metal/RenderTargetView.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/RenderUtils.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../ConsoleRig/Console.h"
#include "../SceneEngine/Techniques.h"

#include "../Utility/StringFormat.h"

#include <functional>       // for std::ref


namespace SceneEngine
{
    using namespace RenderCore;
    using namespace RenderCore::Metal;

    VolumetricFogMaterial VolumetricFogMaterial_Default();
    VolumetricFogMaterial GlobalVolumetricFogMaterial = VolumetricFogMaterial_Default();

        ///////////////////////////////////////////////////////////////////////////////////////////////

    class VolumetricFogShaders
    {
    public:
        class Desc
        {
        public:
            unsigned    _msaaSampleCount;
            bool        _useMsaaSamplers;
            bool        _flipDirection;
            bool        _esmShadowMaps;
            bool        _doNoiseOffset;
            Desc(unsigned msaaSampleCount, bool useMsaaSamplers, bool flipDirection, bool esmShadowMaps, bool doNoiseOffset)
                : _msaaSampleCount(msaaSampleCount), _useMsaaSamplers(useMsaaSamplers), _flipDirection(flipDirection)
                , _esmShadowMaps(esmShadowMaps), _doNoiseOffset(doNoiseOffset) {}
        };

        RenderCore::Metal::ShaderProgram*                   _buildExponentialShadowMap;
        std::unique_ptr<RenderCore::Metal::BoundUniforms>   _buildExponentialShadowMapBinding;
        RenderCore::Metal::ShaderProgram*                   _horizontalFilter;
        RenderCore::Metal::ShaderProgram*                   _verticalFilter;
        std::unique_ptr<RenderCore::Metal::BoundUniforms>   _horizontalFilterBinding, _verticalFilterBinding;

        RenderCore::Metal::ComputeShader*                   _injectLight;
        std::unique_ptr<RenderCore::Metal::BoundUniforms>   _injectLightBinding;
        RenderCore::Metal::ComputeShader*                   _propagateLight;
        std::unique_ptr<RenderCore::Metal::BoundUniforms>   _propagateLightBinding;
        RenderCore::Metal::ShaderProgram*                   _resolveLight;
        std::unique_ptr<RenderCore::Metal::BoundUniforms>   _resolveLightBinding;

        VolumetricFogShaders(const Desc& desc);
        ~VolumetricFogShaders();

        const Assets::DependencyValidation& GetDependancyValidation() const   { return *_validationCallback; }
    private:
        std::shared_ptr<Assets::DependencyValidation>  _validationCallback;
    };

    VolumetricFogShaders::VolumetricFogShaders(const Desc& desc)
    {
        char defines[256];
        _snprintf_s(defines, _TRUNCATE, "ESM_SHADOW_MAPS=%i;DO_NOISE_OFFSET=%i", int(desc._esmShadowMaps), int(desc._doNoiseOffset));
        auto* buildExponentialShadowMap = &Assets::GetAssetDep<ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", "game/xleres/VolumetricEffect/shadowsfilter.psh:BuildExponentialShadowMap:ps_*", defines);
        auto* horizontalFilter = &Assets::GetAssetDep<ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", "game/xleres/VolumetricEffect/shadowsfilter.psh:HorizontalBoxFilter11:ps_*");
        auto* verticalFilter = &Assets::GetAssetDep<ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", "game/xleres/VolumetricEffect/shadowsfilter.psh:VerticalBoxFilter11:ps_*");

        auto buildExponentialShadowMapBinding = std::make_unique<BoundUniforms>(std::ref(*buildExponentialShadowMap));
        TechniqueContext::BindGlobalUniforms(*buildExponentialShadowMapBinding);
        buildExponentialShadowMapBinding->BindConstantBuffer(Hash64("VolumetricFogConstants"), 0, 1);
        buildExponentialShadowMapBinding->BindConstantBuffer(Hash64("$Globals"), 1, 1);

        auto horizontalFilterBinding = std::make_unique<BoundUniforms>(std::ref(*horizontalFilter));
        horizontalFilterBinding->BindConstantBuffer(Hash64("$Globals"), 0, 1);
        horizontalFilterBinding->BindConstantBuffer(Hash64("Filtering"), 1, 1);

        auto verticalFilterBinding = std::make_unique<BoundUniforms>(std::ref(*verticalFilter));
        verticalFilterBinding->BindConstantBuffer(Hash64("$Globals"), 0, 1);
        verticalFilterBinding->BindConstantBuffer(Hash64("Filtering"), 1, 1);

        auto* injectLight = &Assets::GetAssetDep<ComputeShader>("game/xleres/volumetriceffect/injectlight.csh:InjectLighting:cs_*", defines);
        auto* propagateLight = &Assets::GetAssetDep<ComputeShader>("game/xleres/volumetriceffect/injectlight.csh:PropagateLighting:cs_*", defines);

        auto injectLightBinding = std::make_unique<BoundUniforms>(std::ref(Assets::GetAssetDep<CompiledShaderByteCode>("game/xleres/volumetriceffect/injectlight.csh:InjectLighting:cs_*", defines)));
        TechniqueContext::BindGlobalUniforms(*injectLightBinding);
        injectLightBinding->BindConstantBuffer(Hash64("VolumetricFogConstants"), 0, 1);
        injectLightBinding->BindConstantBuffer(Hash64("ArbitraryShadowProjection"), 2, 1);

        auto propagateLightBinding = std::make_unique<BoundUniforms>(std::ref(Assets::GetAssetDep<CompiledShaderByteCode>("game/xleres/volumetriceffect/injectlight.csh:PropagateLighting:cs_*", defines)));
        TechniqueContext::BindGlobalUniforms(*propagateLightBinding);
        propagateLightBinding->BindConstantBuffer(Hash64("VolumetricFogConstants"), 0, 1);
        injectLightBinding->BindConstantBuffer(Hash64("ArbitraryShadowProjection"), 2, 1);

        const char* vertexShader = 
            desc._flipDirection
                ? "game/xleres/basic2D.vsh:fullscreen_flip:vs_*"
                : "game/xleres/basic2D.vsh:fullscreen:vs_*"
                ;
        char definesTable[256];
        Utility::XlFormatString(
            definesTable, dimof(definesTable), 
            "MSAA_SAMPLES=%i", 
            (desc._msaaSampleCount<=1)?0:desc._msaaSampleCount);
        if (desc._useMsaaSamplers) {
            XlCatString(definesTable, dimof(definesTable), ";MSAA_SAMPLERS=1");
        }
        auto* resolveLight = &Assets::GetAssetDep<ShaderProgram>(
            vertexShader, "game/xleres/VolumetricEffect/resolvefog.psh:ResolveFog:ps_*", definesTable);

        auto resolveLightBinding = std::make_unique<BoundUniforms>(std::ref(*resolveLight));
        resolveLightBinding->BindConstantBuffer(Hash64("GlobalTransform"), 0, 0);

        auto validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(validationCallback, &buildExponentialShadowMap->GetDependancyValidation());
        Assets::RegisterAssetDependency(validationCallback, &horizontalFilter->GetDependancyValidation());
        Assets::RegisterAssetDependency(validationCallback, &verticalFilter->GetDependancyValidation());
        Assets::RegisterAssetDependency(validationCallback, &injectLight->GetDependancyValidation());
        Assets::RegisterAssetDependency(validationCallback, &propagateLight->GetDependancyValidation());
        Assets::RegisterAssetDependency(validationCallback, &resolveLight->GetDependancyValidation());

            //  Commit pointers
        _buildExponentialShadowMap = std::move(buildExponentialShadowMap);
        _horizontalFilter = std::move(horizontalFilter);
        _verticalFilter = std::move(verticalFilter);

        _buildExponentialShadowMapBinding = std::move(buildExponentialShadowMapBinding);
        _horizontalFilterBinding = std::move(horizontalFilterBinding);
        _verticalFilterBinding = std::move(verticalFilterBinding);

        _injectLight = injectLight;
        _propagateLight = propagateLight;
        _resolveLight = resolveLight;
        _resolveLightBinding = std::move(resolveLightBinding);
        _validationCallback = std::move(validationCallback);
        _injectLightBinding = std::move(injectLightBinding);
        _propagateLightBinding = std::move(propagateLightBinding);
    }

    VolumetricFogShaders::~VolumetricFogShaders()
    {}

    ///////////////////////////////////////////////////////////////////////////////////////////////

    class VolumetricFogResources
    {
    public:
        class Desc
        {
        public:
            int     _frustumCount;
            bool    _esmShadowMaps;
            Desc(int frustumCount, bool esmShadowMaps) : _frustumCount(frustumCount), _esmShadowMaps(esmShadowMaps) {}
        };

        intrusive_ptr<ID3D::Resource>                          _shadowMapTexture;
        std::vector<RenderCore::Metal::RenderTargetView>    _shadowMapRenderTargets;
        RenderCore::Metal::ShaderResourceView               _shadowMapShaderResource;

        intrusive_ptr<ID3D::Resource>                          _shadowMapTextureTemp;
        std::vector<RenderCore::Metal::RenderTargetView>    _shadowMapRenderTargetsTemp;
        RenderCore::Metal::ShaderResourceView               _shadowMapShaderResourceTemp;

        intrusive_ptr<ID3D::Resource>                          _densityValuesTexture;
        RenderCore::Metal::UnorderedAccessView              _densityValuesUnorderedAccess;
        RenderCore::Metal::ShaderResourceView               _densityValuesShaderResource;

        intrusive_ptr<ID3D::Resource>                          _inscatterShadowingValuesTexture;
        RenderCore::Metal::UnorderedAccessView              _inscatterShadowingValuesUnorderedAccess;
        RenderCore::Metal::ShaderResourceView               _inscatterShadowingValuesShaderResource;

        intrusive_ptr<ID3D::Resource>                          _inscatterPointLightsValuesTexture;
        RenderCore::Metal::UnorderedAccessView              _inscatterPointLightsValuesUnorderedAccess;
        RenderCore::Metal::ShaderResourceView               _inscatterPointLightsValuesShaderResource;

        intrusive_ptr<ID3D::Resource>                          _inscatterFinalsValuesTexture;
        RenderCore::Metal::UnorderedAccessView              _inscatterFinalsValuesUnorderedAccess;
        RenderCore::Metal::ShaderResourceView               _inscatterFinalsValuesShaderResource;

        intrusive_ptr<ID3D::Resource>                          _transmissionValuesTexture;
        RenderCore::Metal::UnorderedAccessView              _transmissionValuesUnorderedAccess;
        RenderCore::Metal::ShaderResourceView               _transmissionValuesShaderResource;

        VolumetricFogResources(const Desc& desc);
        ~VolumetricFogResources();
    };

    VolumetricFogResources::VolumetricFogResources(const Desc& desc)
    {
        auto shadowMapFormat = desc._esmShadowMaps?NativeFormat::R32_FLOAT:NativeFormat::R16_UNORM;
        auto renderTargetDesc = BuildRenderTargetDesc(BufferUploads::BindFlag::RenderTarget|BufferUploads::BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain2D(256, 256, shadowMapFormat, 0, uint8(desc._frustumCount)));
        auto& uploads = *GetBufferUploads();

        auto shadowMapTexture = uploads.Transaction_Immediate(renderTargetDesc, nullptr)->AdoptUnderlying();
        auto shadowMapShaderResource = ShaderResourceView(shadowMapTexture.get(), shadowMapFormat, desc._frustumCount);
        std::vector<RenderTargetView> shadowMapRenderTargets;
        for (int c=0; c<desc._frustumCount; ++c) {
            shadowMapRenderTargets.emplace_back(
                RenderTargetView(shadowMapTexture.get(), shadowMapFormat, ArraySlice(1, c)));
        }

        auto shadowMapTextureTemp = uploads.Transaction_Immediate(renderTargetDesc, nullptr)->AdoptUnderlying();
        auto shadowMapShaderResourceTemp = ShaderResourceView(shadowMapTextureTemp.get(), shadowMapFormat, desc._frustumCount);
        std::vector<RenderTargetView> shadowMapRenderTargetsTemp;
        for (int c=0; c<desc._frustumCount; ++c) {
            shadowMapRenderTargetsTemp.emplace_back(
                RenderTargetView(shadowMapTextureTemp.get(), shadowMapFormat, ArraySlice(1, c)));
        }

        auto densityTextureDesc = BuildRenderTargetDesc(BufferUploads::BindFlag::UnorderedAccess|BufferUploads::BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain3D(160, 90, 128, NativeFormat::R32_TYPELESS));
        auto densityTexture = uploads.Transaction_Immediate(densityTextureDesc, nullptr)->AdoptUnderlying();
        UnorderedAccessView densityUnorderedAccess(densityTexture.get(), NativeFormat::R32_FLOAT);
        ShaderResourceView densityShaderResource(densityTexture.get(), NativeFormat::R32_FLOAT);

        auto inscatterShadowingTexture = uploads.Transaction_Immediate(densityTextureDesc, nullptr)->AdoptUnderlying();
        UnorderedAccessView inscatterShadowingUnorderedAccess(inscatterShadowingTexture.get(), NativeFormat::R32_FLOAT);
        ShaderResourceView inscatterShadowingShaderResource(inscatterShadowingTexture.get(), NativeFormat::R32_FLOAT);
    
        auto transmissionTexture = uploads.Transaction_Immediate(densityTextureDesc, nullptr)->AdoptUnderlying();
        UnorderedAccessView transmissionUnorderedAccess(transmissionTexture.get(), NativeFormat::R32_FLOAT);
        ShaderResourceView transmissionShaderResource(transmissionTexture.get(), NativeFormat::R32_FLOAT);

        auto scatteringTextureDesc = BuildRenderTargetDesc(BufferUploads::BindFlag::UnorderedAccess|BufferUploads::BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain3D(160, 90, 128, NativeFormat::R32G32B32A32_TYPELESS));
        auto inscatterFinalsTexture = uploads.Transaction_Immediate(scatteringTextureDesc, nullptr)->AdoptUnderlying();
        UnorderedAccessView inscatterFinalsUnorderedAccess(inscatterFinalsTexture.get(), NativeFormat::R32G32B32A32_FLOAT);
        ShaderResourceView inscatterFinalsShaderResource(inscatterFinalsTexture.get(), NativeFormat::R32G32B32A32_FLOAT);

        auto inscatterPointLightsTexture = uploads.Transaction_Immediate(scatteringTextureDesc, nullptr)->AdoptUnderlying();
        UnorderedAccessView inscatterPointLightsUnorderedAccess(inscatterPointLightsTexture.get(), NativeFormat::R32G32B32A32_FLOAT);
        ShaderResourceView inscatterPointLightsShaderResource(inscatterPointLightsTexture.get(), NativeFormat::R32G32B32A32_FLOAT);

            //  Commit pointers
        _densityValuesTexture = std::move(densityTexture);
        _densityValuesUnorderedAccess = std::move(densityUnorderedAccess);
        _densityValuesShaderResource = std::move(densityShaderResource);

        _inscatterShadowingValuesTexture = std::move(inscatterShadowingTexture);
        _inscatterShadowingValuesUnorderedAccess = std::move(inscatterShadowingUnorderedAccess);
        _inscatterShadowingValuesShaderResource = std::move(inscatterShadowingShaderResource);

        _inscatterPointLightsValuesTexture = std::move(inscatterPointLightsTexture);
        _inscatterPointLightsValuesUnorderedAccess = std::move(inscatterPointLightsUnorderedAccess);
        _inscatterPointLightsValuesShaderResource = std::move(inscatterPointLightsShaderResource);

        _inscatterFinalsValuesTexture = std::move(inscatterFinalsTexture);
        _inscatterFinalsValuesUnorderedAccess = std::move(inscatterFinalsUnorderedAccess);
        _inscatterFinalsValuesShaderResource = std::move(inscatterFinalsShaderResource);

        _transmissionValuesTexture = std::move(transmissionTexture);
        _transmissionValuesUnorderedAccess = std::move(transmissionUnorderedAccess);
        _transmissionValuesShaderResource = std::move(transmissionShaderResource);

        _shadowMapTexture = std::move(_shadowMapTexture);
        _shadowMapRenderTargets = std::move(shadowMapRenderTargets);
        _shadowMapShaderResource = std::move(shadowMapShaderResource);
        _shadowMapTextureTemp = std::move(_shadowMapTextureTemp);
        _shadowMapRenderTargetsTemp = std::move(shadowMapRenderTargetsTemp);
        _shadowMapShaderResourceTemp = std::move(shadowMapShaderResourceTemp);
    }

    VolumetricFogResources::~VolumetricFogResources()
    {}

    static bool UseESMShadowMaps() { return Tweakable("VolFogESM", true); }

    RenderCore::Metal::ConstantBufferPacket MakeVolFogConstants()
    {
        auto& mat = GlobalVolumetricFogMaterial;
        struct Constants
        {
            float ESM_C;
            float ShadowsBias;
            float JitteringAmount;
            float Density;
            float NoiseDensityScale;
            float NoiseSpeed;
            float HeightStart;
            float HeightEnd;

            Float3 ForwardColour; unsigned _pad1;
            Float3 BackColour; unsigned _pad2;
        } constants = {
            mat._ESM_C, mat._shadowsBias, mat._jitteringAmount, mat._density, 
            mat._noiseDensityScale, mat._noiseSpeed,
            mat._heightStart, mat._heightEnd,
            mat._forwardBrightness * mat._forwardColour, 0,
            mat._backBrightness * mat._backColour, 0
        };

        return RenderCore::MakeSharedPkt(constants);
    }

    void VolumetricFog_Build(   RenderCore::Metal::DeviceContext* context, 
                                LightingParserContext& lightingParserContext,
                                bool useMsaaSamplers, 
                                PreparedShadowFrustum& shadowFrustum)
    {
        TRY {
            auto& fogRes = FindCachedBox<VolumetricFogResources>(VolumetricFogResources::Desc(shadowFrustum._frustumCount, UseESMShadowMaps()));
            auto& fogShaders = FindCachedBoxDep<VolumetricFogShaders>(VolumetricFogShaders::Desc(1, useMsaaSamplers, false, UseESMShadowMaps(), 
                GlobalVolumetricFogMaterial._noiseDensityScale > 0.f));

            RenderCore::Metal::ConstantBufferPacket constantBufferPackets[3];
            constantBufferPackets[0] = MakeVolFogConstants();

            SavedTargets savedTargets(context);
            ViewportDesc newViewport(0, 0, 256, 256, 0.f, 1.f);
            context->Bind(newViewport);
            context->Bind(CommonResources()._blendOpaque);
            context->Bind(*fogShaders._buildExponentialShadowMap);
            SetupVertexGeneratorShader(context);
            context->BindPS(MakeResourceList(2, shadowFrustum._shadowTextureResource));

            int c=0; 
            for (auto i=fogRes._shadowMapRenderTargets.begin(); i!=fogRes._shadowMapRenderTargets.end(); ++i, ++c) {

                struct WorkingSlice { int _workingSlice; float _depthProjRatio0; float _depthProjRatio1; } globalsBuffer = {
                    c, shadowFrustum._shadowProjRatio[c][0], shadowFrustum._projectConstantBufferSource._shadowProjRatio[c][1]
                };
                constantBufferPackets[1] = MakeSharedPkt(globalsBuffer);

                fogShaders._buildExponentialShadowMapBinding->Apply(
                    *context, lightingParserContext.GetGlobalUniformsStream(),
                    UniformsStream(constantBufferPackets, nullptr, dimof(constantBufferPackets)));

                context->Bind(MakeResourceList(*i), nullptr);
                context->Draw(4);
            }

            float filteringWeights[8];
            static float standardDeviation = 1.6f; // 0.809171316279f; // 1.6f;
            BuildGaussianFilteringWeights(filteringWeights, standardDeviation, dimof(filteringWeights));
            auto filteringWeightsConstants = MakeSharedPkt(filteringWeights);

            c = 0;
            auto i2 = fogRes._shadowMapRenderTargetsTemp.begin();
            for (auto i=fogRes._shadowMapRenderTargets.begin(); i!=fogRes._shadowMapRenderTargets.end(); ++i, ++c, ++i2) {

                struct WorkingSlice { int _workingSlice; } globalsBuffer = { c };
                RenderCore::Metal::ConstantBufferPacket constantBufferPackets[2];
                constantBufferPackets[0] = MakeSharedPkt(globalsBuffer);
                constantBufferPackets[1] = filteringWeightsConstants;
                fogShaders._horizontalFilterBinding->Apply(
                    *context, lightingParserContext.GetGlobalUniformsStream(),
                    UniformsStream(constantBufferPackets, nullptr, dimof(constantBufferPackets)));

                context->UnbindPS<ShaderResourceView>(0, 1);
                context->Bind(MakeResourceList(*i2), nullptr);
                context->BindPS(MakeResourceList(fogRes._shadowMapShaderResource));
                context->Bind(*fogShaders._horizontalFilter);
                context->Draw(4);

                context->UnbindPS<ShaderResourceView>(0, 1);
                context->Bind(MakeResourceList(*i), nullptr);
                context->BindPS(MakeResourceList(fogRes._shadowMapShaderResourceTemp));
                context->Bind(*fogShaders._verticalFilter);
                context->Draw(4);
            }

            savedTargets.ResetToOldTargets(context);
            context->Bind(CommonResources()._blendStraightAlpha);

            auto& perlinNoiseRes = FindCachedBox<PerlinNoiseResources>(PerlinNoiseResources::Desc());
            context->BindCS(MakeResourceList(12, perlinNoiseRes._gradShaderResource, perlinNoiseRes._permShaderResource));

                //  Inject the starting light into the volume texture (using compute shader)
            context->BindCS(MakeResourceList(
                fogRes._inscatterShadowingValuesUnorderedAccess, fogRes._transmissionValuesUnorderedAccess,
                fogRes._densityValuesUnorderedAccess, fogRes._inscatterFinalsValuesUnorderedAccess));
            context->BindCS(MakeResourceList(2, fogRes._shadowMapShaderResource));
            context->BindCS(MakeResourceList(9, Assets::GetAssetDep<Metal::DeferredShaderResource>("game/xleres/DefaultResources/balanced_noise.dds").GetShaderResource()));
            // context->BindCS(MakeResourceList(
            //     lightingParserContext.GetGlobalTransformCB(), Metal::ConstantBuffer(), Metal::ConstantBuffer(), 
            //     *shadowFrustum._projectConstantBuffer, lightingParserContext.GetGlobalStateCB()));

            const Metal::ConstantBuffer* prebuiltCBs[3] = { nullptr, nullptr, &shadowFrustum._arbitraryCB };
            fogShaders._injectLightBinding->Apply(
                *context, lightingParserContext.GetGlobalUniformsStream(),
                UniformsStream(constantBufferPackets, prebuiltCBs, dimof(constantBufferPackets)));

            context->BindCS(MakeResourceList(Metal::SamplerState()));
            context->Bind(*fogShaders._injectLight);
            context->Dispatch(160/10, 90/10, 128/8);

            context->UnbindCS<UnorderedAccessView>(0, 1);

            fogShaders._propagateLightBinding->Apply(
                *context, lightingParserContext.GetGlobalUniformsStream(),
                UniformsStream(constantBufferPackets, prebuiltCBs, dimof(constantBufferPackets)));

            context->BindCS(MakeResourceList(3, fogRes._inscatterShadowingValuesShaderResource, fogRes._inscatterPointLightsValuesShaderResource));
            context->Bind(*fogShaders._propagateLight);
            context->Dispatch(160/10, 90/10, 1);

            context->UnbindCS<UnorderedAccessView>(0, 4);
            context->UnbindCS<ShaderResourceView>(2, 3);
            context->UnbindCS<ShaderResourceView>(13, 3);
            context->Unbind<ComputeShader>();
        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { lightingParserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { lightingParserContext.Process(e); }
        CATCH_END
    }

    void VolumetricFog_Resolve( RenderCore::Metal::DeviceContext* context, 
                                LightingParserContext& lightingParserContext,
                                unsigned samplingCount, bool useMsaaSamplers, bool flipDirection,
                                PreparedShadowFrustum& shadowFrustum)
    {
        auto& fogRes = FindCachedBox<VolumetricFogResources>(VolumetricFogResources::Desc(shadowFrustum._frustumCount, UseESMShadowMaps()));
        auto& fogShaders = FindCachedBoxDep<VolumetricFogShaders>(VolumetricFogShaders::Desc(samplingCount, useMsaaSamplers, flipDirection, UseESMShadowMaps(), 
            GlobalVolumetricFogMaterial._noiseDensityScale > 0.f));

        context->BindPS(MakeResourceList(7, fogRes._inscatterFinalsValuesShaderResource, fogRes._transmissionValuesShaderResource));
        fogShaders._resolveLightBinding->Apply(*context, lightingParserContext.GetGlobalUniformsStream(), UniformsStream());
        context->Bind(*fogShaders._resolveLight);
        context->Draw(4);
    }

        ///////////////////////////////////////////////////////////////////////////////////////////////

    VolumetricFogMaterial VolumetricFogMaterial_Default()
    {
        VolumetricFogMaterial result;
        result._density = .5f;
        result._noiseDensityScale = 0.f;
        result._noiseSpeed = 1.f;
        result._heightStart = 270.f;
        result._heightEnd = 170.f;
        result._forwardColour = Float3(.7f, .6f, 1.f);
        result._backColour = Float3(0.5f, 0.5f, .65f);
        result._forwardBrightness = 17.f;
        result._backBrightness = 17.f;
        result._ESM_C = .25f * 80.f;
        result._shadowsBias = 0.00000125f;
        result._jitteringAmount = 0.5f;
        return result;
    }

        ///////////////////////////////////////////////////////////////////////////////////////////////

#if 0
    class AirLightResources
    {
    public:
        class Desc {};

        AirLightResources(const Desc& desc);
        ~AirLightResources();

        intrusive_ptr<ID3D::Resource>                          _lookupTexture;
        RenderCore::Metal::ShaderResourceView               _lookupShaderResource;
    };

    static float AirLightF(float u, float v)
    {
            //
            // see A Practical Analytic Single Scattering Model for Real Time Rendering
            //      paper from Columbia university
            //      equation 10.

        float A = 1.f; // XlExp(-u * XlTan(0.f));
        const unsigned steps = 256;
        const float stepWidth = v/float(steps);

        float result = 0.f;
        for (unsigned c=0; c<steps; ++c) {
            float e = (c+1)*stepWidth;
            float B = XlExp(-u * XlTan(e));
            result += (A + B) * .5f * stepWidth;
        }
        return result;
    }

    AirLightResources::AirLightResources(const Desc& desc)
    {
        const unsigned lookupTableDimensions = 256;
        float lookupTableData[lookupTableDimensions*lookupTableDimensions];
        const float uMax = 10.f;
        const float vMax = gPI * 0.5f;
        for (unsigned y=0; y<lookupTableDimensions; ++y) {
            for (unsigned x=0; x<lookupTableDimensions; ++x) {
                const float u = x*uMax/float(lookupTableDimensions-1);
                const float v = y*vMax/float(lookupTableDimensions-1);
            }
        }

        auto& uploads = *GetBufferUploads();
        auto lookupDesc = BuildRenderTargetDesc(BufferUploads::BindFlag::ShaderResource, BufferUploads::TextureDesc::Plain2D(lookupTableDimensions, lookupTableDimensions, NativeFormat::R32_TYPELESS));
        intrusive_ptr<ID3D::Resource> lookupTexture((ID3D::Resource*)uploads.Transaction_Immediate(
            lookupDesc, lookupTableData, lookupTableDimensions*lookupTableDimensions*sizeof(float), 
            std::make_pair(lookupTableDimensions*sizeof(float), lookupTableDimensions*lookupTableDimensions*sizeof(float)))._resource, false);
        ShaderResourceView lookupShaderResource(lookupTexture.get(), NativeFormat::R32_FLOAT);

        _lookupTexture = std::move(lookupTexture);
        _lookupShaderResource = std::move(lookupShaderResource);
    }

    AirLightResources::~AirLightResources()
    {}
#endif

}

