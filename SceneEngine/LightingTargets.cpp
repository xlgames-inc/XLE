// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LightingTargets.h"
#include "SceneEngineUtility.h"
#include "LightingParserContext.h"

#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/StringFormat.h"

#include "../RenderCore/DX11/Metal/IncludeDX11.h"
#if defined(_DEBUG)
    #include <D3DX11tex.h>      // for saving the gbuffer
#endif

namespace SceneEngine
{

    MainTargetsBox::Desc::Desc( unsigned width, unsigned height, 
                                const FormatStack& diffuseFormat, const FormatStack& normalFormat, 
                                const FormatStack& parametersFormat, const FormatStack& depthFormat,
                                const BufferUploads::TextureSamples& sampling)
    {
            //  we have to "memset" this -- because padding adds 
            //  random values in profile mode
        std::fill((char*)this, PtrAdd((char*)this, sizeof(*this)), 0);

        _width = width; _height = height;
        _gbufferFormats[0] = diffuseFormat;
        _gbufferFormats[1] = normalFormat;
        _gbufferFormats[2] = parametersFormat;
        _depthFormat = depthFormat;
        _sampling = sampling;
    }

    MainTargetsBox::MainTargetsBox(const Desc& desc) 
    : _desc(desc)
    {
        using namespace RenderCore;
        using namespace RenderCore::Metal;
        using namespace BufferUploads;

        ResourcePtr gbufferTextures[s_gbufferTextureCount];
        RenderTargetView gbufferRTV[dimof(gbufferTextures)];
        ShaderResourceView gbufferSRV[dimof(gbufferTextures)];
        std::fill(gbufferTextures, &gbufferTextures[dimof(gbufferTextures)], nullptr);

        auto bufferUploadsDesc = BufferUploads::CreateDesc(
            BindFlag::ShaderResource|BindFlag::RenderTarget,
            0, GPUAccess::Write | GPUAccess::Read,
            BufferUploads::TextureDesc::Plain2D(
                desc._width, desc._height, AsDXGIFormat(NativeFormat::Unknown), 1, 0, desc._sampling),
            "GBuffer");
        for (unsigned c=0; c<dimof(gbufferTextures); ++c) {
            if (desc._gbufferFormats[c]._resourceFormat != NativeFormat::Unknown) {
                bufferUploadsDesc._textureDesc._nativePixelFormat = AsDXGIFormat(desc._gbufferFormats[c]._resourceFormat);
                gbufferTextures[c] = CreateResourceImmediate(bufferUploadsDesc);
                gbufferRTV[c] = RenderTargetView(gbufferTextures[c].get(), desc._gbufferFormats[c]._writeFormat);
                gbufferSRV[c] = ShaderResourceView(gbufferTextures[c].get(), desc._gbufferFormats[c]._shaderReadFormat);
            }
        }

            /////////
        
        auto depthBufferDesc = BufferUploads::CreateDesc(
            BindFlag::ShaderResource|BindFlag::DepthStencil,
            0, GPUAccess::Write | GPUAccess::Read,
            BufferUploads::TextureDesc::Plain2D(
                desc._width, desc._height, AsDXGIFormat(desc._depthFormat._resourceFormat), 1, 0, desc._sampling),
            "MainDepth");
        auto msaaDepthBufferTexture = CreateResourceImmediate(depthBufferDesc);
        auto secondaryDepthBufferTexture = CreateResourceImmediate(depthBufferDesc);
        DepthStencilView msaaDepthBuffer(msaaDepthBufferTexture.get(), desc._depthFormat._writeFormat);
        DepthStencilView secondaryDepthBuffer(secondaryDepthBufferTexture.get(), desc._depthFormat._writeFormat);
        ShaderResourceView msaaDepthBufferSRV(msaaDepthBufferTexture.get(), desc._depthFormat._shaderReadFormat);
        ShaderResourceView secondaryDepthBufferSRV(secondaryDepthBufferTexture.get(), desc._depthFormat._shaderReadFormat);

            /////////

        for (unsigned c=0; c<dimof(_gbufferTextures); ++c) {
            _gbufferTextures[c] = std::move(gbufferTextures[c]);
            _gbufferRTVs[c] = std::move(gbufferRTV[c]);
            _gbufferRTVsSRV[c] = std::move(gbufferSRV[c]);
        }
        _msaaDepthBufferTexture = std::move(msaaDepthBufferTexture);
        _secondaryDepthBufferTexture = std::move(secondaryDepthBufferTexture);
        _msaaDepthBuffer = std::move(msaaDepthBuffer);
        _secondaryDepthBuffer = std::move(secondaryDepthBuffer);
        _msaaDepthBufferSRV = std::move(msaaDepthBufferSRV);
        _secondaryDepthBufferSRV = std::move(secondaryDepthBufferSRV);
    }

    MainTargetsBox::~MainTargetsBox() {}

    ///////////////////////////////////////////////////////////////////////////////////////////////

    ForwardTargetsBox::Desc::Desc( unsigned width, unsigned height, 
                                const FormatStack& depthFormat,
                                const BufferUploads::TextureSamples& sampling)
    {
            //  we have to "memset" this -- because padding adds random values in 
            //  profile mode
        std::fill((char*)this, PtrAdd((char*)this, sizeof(*this)), 0);

        _width = width; _height = height;
        _depthFormat = depthFormat;
        _sampling = sampling;
    }

    ForwardTargetsBox::ForwardTargetsBox(const Desc& desc) 
    : _desc(desc)
    {
        using namespace RenderCore;
        using namespace RenderCore::Metal;
        using namespace BufferUploads;
        auto bufferUploadsDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource|BindFlag::DepthStencil,
            BufferUploads::TextureDesc::Plain2D(
                desc._width, desc._height, AsDXGIFormat(desc._depthFormat._resourceFormat), 1, 0, desc._sampling),
            "ForwardTarget");

        auto msaaDepthBufferTexture = CreateResourceImmediate(bufferUploadsDesc);
        auto secondaryDepthBufferTexture = CreateResourceImmediate(bufferUploadsDesc);

            /////////

        DepthStencilView msaaDepthBuffer(msaaDepthBufferTexture.get(), desc._depthFormat._writeFormat);
        DepthStencilView secondaryDepthBuffer(secondaryDepthBufferTexture.get(), desc._depthFormat._writeFormat);

        ShaderResourceView msaaDepthBufferSRV(msaaDepthBufferTexture.get(), desc._depthFormat._shaderReadFormat);
        ShaderResourceView secondaryDepthBufferSRV(secondaryDepthBufferTexture.get(), desc._depthFormat._shaderReadFormat);

            /////////

        _msaaDepthBufferTexture = std::move(msaaDepthBufferTexture);
        _secondaryDepthBufferTexture = std::move(secondaryDepthBufferTexture);

        _msaaDepthBuffer = std::move(msaaDepthBuffer);
        _secondaryDepthBuffer = std::move(secondaryDepthBuffer);

        _msaaDepthBufferSRV = std::move(msaaDepthBufferSRV);
        _secondaryDepthBufferSRV = std::move(secondaryDepthBufferSRV);
    }

    ForwardTargetsBox::~ForwardTargetsBox() {}

    ///////////////////////////////////////////////////////////////////////////////////////////////

    LightingResolveTextureBox::Desc::Desc( unsigned width, unsigned height, 
                                const FormatStack& lightingResolveFormat,
                                const BufferUploads::TextureSamples& sampling)
    {
            //  we have to "memset" this -- because padding adds 
            //  random values in profile mode
        std::fill((char*)this, PtrAdd((char*)this, sizeof(*this)), 0);

        _width = width; _height = height;
        _lightingResolveFormat = lightingResolveFormat;
        _sampling = sampling;
    }
    
    LightingResolveTextureBox::LightingResolveTextureBox(const Desc& desc)
    {
        using namespace RenderCore;
        using namespace RenderCore::Metal;
        using namespace BufferUploads;
        auto bufferUploadsDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource|BindFlag::RenderTarget,
            BufferUploads::TextureDesc::Plain2D(
                desc._width, desc._height, AsDXGIFormat(desc._lightingResolveFormat._resourceFormat), 1, 0, 
                desc._sampling),
            "LightResolve");

        auto lightingResolveTexture = CreateResourceImmediate(bufferUploadsDesc);
        auto lightingResolveCopy = CreateResourceImmediate(bufferUploadsDesc);
        bufferUploadsDesc._textureDesc._samples = TextureSamples::Create();

        RenderTargetView lightingResolveTarget(lightingResolveTexture.get(), desc._lightingResolveFormat._writeFormat);
        ShaderResourceView lightingResolveSRV(lightingResolveTexture.get(), desc._lightingResolveFormat._shaderReadFormat);
        ShaderResourceView lightingResolveCopySRV(lightingResolveCopy.get(), desc._lightingResolveFormat._shaderReadFormat);

        _lightingResolveTexture = std::move(lightingResolveTexture);
        _lightingResolveRTV = std::move(lightingResolveTarget);
        _lightingResolveSRV = std::move(lightingResolveSRV);

        _lightingResolveCopy = std::move(lightingResolveCopy);
        _lightingResolveCopySRV = std::move(lightingResolveCopySRV);
    }

    LightingResolveTextureBox::~LightingResolveTextureBox()
    {
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    LightingResolveShaders::LightingResolveShaders(const Desc& desc)
    {
        using namespace RenderCore;

        char definesTable[256];
        Utility::XlFormatString(
            definesTable, dimof(definesTable), 
            "GBUFFER_TYPE=%i;MSAA_SAMPLES=%i", 
            desc._gbufferType, (desc._msaaSampleCount<=1)?0:desc._msaaSampleCount);

        if (desc._msaaSamplers) {
            XlCatString(definesTable, dimof(definesTable), ";MSAA_SAMPLERS=1");
        }

        const char* vertexShader_viewFrustumVector = 
            desc._flipDirection
                ? "game/xleres/basic2D.vsh:fullscreen_flip_viewfrustumvector:vs_*"
                : "game/xleres/basic2D.vsh:fullscreen_viewfrustumvector:vs_*"
                ;

        _shadowedDirectionalLight = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            vertexShader_viewFrustumVector, 
            "game/xleres/deferred/resolve.psh:ResolveLight:ps_*",
            StringMeld<256>() << definesTable << ";SHADOW_CASCADE_MODE=1");
        _shadowedDirectionalOrthoLight = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            vertexShader_viewFrustumVector, 
            "game/xleres/deferred/resolve.psh:ResolveLight:ps_*",
            StringMeld<256>() << definesTable << ";SHADOW_CASCADE_MODE=2");
        _shadowedPointLight = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            vertexShader_viewFrustumVector, 
            "game/xleres/deferred/resolve.psh:ResolvePointLight:ps_*",
            StringMeld<256>() << definesTable << ";SHADOW_CASCADE_MODE=1");

        _unshadowedDirectionalLight = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            vertexShader_viewFrustumVector, 
            "game/xleres/deferred/resolveunshadowed.psh:ResolveLightUnshadowed:ps_*",
            definesTable);
        _unshadowedPointLight = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            vertexShader_viewFrustumVector, 
            "game/xleres/deferred/resolveunshadowed.psh:ResolvePointLightUnshadowed:ps_*",
            definesTable);

        std::unique_ptr<BoundUniforms> bu[5];
        bu[0] = std::make_unique<Metal::BoundUniforms>(std::ref(*_shadowedDirectionalLight));
        bu[1] = std::make_unique<Metal::BoundUniforms>(std::ref(*_shadowedDirectionalOrthoLight));
        bu[2] = std::make_unique<Metal::BoundUniforms>(std::ref(*_shadowedPointLight));
        bu[3] = std::make_unique<Metal::BoundUniforms>(std::ref(*_unshadowedDirectionalLight));
        bu[4] = std::make_unique<Metal::BoundUniforms>(std::ref(*_unshadowedPointLight));

        for (unsigned c=0; c<dimof(bu); ++c) {
            Techniques::TechniqueContext::BindGlobalUniforms(*bu[c]);
            bu[c]->BindConstantBuffer(Hash64("ArbitraryShadowProjection"), 0, 1);
            bu[c]->BindConstantBuffer(Hash64("LightBuffer"), 1, 1);
            bu[c]->BindConstantBuffer(Hash64("ShadowParameters"), 2, 1);
            bu[c]->BindConstantBuffer(Hash64("ScreenToShadowProjection"), 3, 1);
            bu[c]->BindConstantBuffer(Hash64("OrthogonalShadowProjection"), 4, 1);
        }

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_validationCallback, &_shadowedDirectionalLight->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, &_shadowedDirectionalOrthoLight->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, &_shadowedPointLight->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, &_unshadowedDirectionalLight->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, &_unshadowedPointLight->GetDependencyValidation());

        _shadowedDirectionalLightUniforms = std::move(bu[0]);
        _shadowedDirectionalOrthoLightUniforms = std::move(bu[1]);
        _shadowedPointLightUniforms = std::move(bu[2]);
        _unshadowedDirectionalLightUniforms = std::move(bu[3]);
        _unshadowedPointLightUniforms = std::move(bu[4]);
    }

    LightingResolveShaders::~LightingResolveShaders() {}

    ///////////////////////////////////////////////////////////////////////////////////////////////

    AmbientResolveShaders::AmbientResolveShaders(const Desc& desc)
    {
        using namespace RenderCore;
        char definesTable[256];
        Utility::XlFormatString(
            definesTable, dimof(definesTable), 
            "MSAA_SAMPLES=%i;MAT_SKY_PROJECTION=%i;CALCULATE_AMBIENT_OCCLUSION=%i;CALCULATE_TILED_LIGHTS=%i;CALCULATE_SCREENSPACE_REFLECTIONS=%i", 
            (desc._msaaSampleCount<=1)?0:desc._msaaSampleCount,
            desc._skyProjectionType, desc._hasAO, desc._hasTiledLighting,desc._hasSRR);

        if (desc._msaaSamplers) {
            XlCatString(definesTable, dimof(definesTable), ";MSAA_SAMPLERS=1");
        }

        const char* vertexShader_viewFrustumVector = 
            desc._flipDirection
                ? "game/xleres/basic2D.vsh:fullscreen_flip_viewfrustumvector:vs_*"
                : "game/xleres/basic2D.vsh:fullscreen_viewfrustumvector:vs_*"
                ;

        auto* ambientLight = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            vertexShader_viewFrustumVector, 
            "game/xleres/deferred/resolveambient.psh:ResolveAmbient:ps_*",
            definesTable);

        auto ambientLightUniforms = std::make_unique<Metal::BoundUniforms>(std::ref(*ambientLight));
        ambientLightUniforms->BindConstantBuffer(Hash64("AmbientLightBuffer"), 0, 1);

        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(validationCallback, &ambientLight->GetDependencyValidation());

        _ambientLight = std::move(ambientLight);
        _ambientLightUniforms = std::move(ambientLightUniforms);
        _validationCallback = std::move(validationCallback);
    }

    AmbientResolveShaders::~AmbientResolveShaders() {}

    ///////////////////////////////////////////////////////////////////////////////////////////////


    #if defined(_DEBUG)
        void SaveGBuffer(RenderCore::Metal::DeviceContext* context, MainTargetsBox& mainTargets)
        {
            using namespace BufferUploads;
            BufferDesc stagingDesc[3];
            for (unsigned c=0; c<3; ++c) {
                stagingDesc[c]._type = BufferDesc::Type::Texture;
                stagingDesc[c]._bindFlags = 0;
                stagingDesc[c]._cpuAccess = CPUAccess::Read;
                stagingDesc[c]._gpuAccess = 0;
                stagingDesc[c]._allocationRules = 0;
                stagingDesc[c]._textureDesc = BufferUploads::TextureDesc::Plain2D(
                    mainTargets._desc._width, mainTargets._desc._height,
                    mainTargets._desc._gbufferFormats[c]._shaderReadFormat, 1, 0, 
                    mainTargets._desc._sampling);
            }

            const char* outputNames[] = { "gbuffer_diffuse.dds", "gbuffer_normals.dds", "gbuffer_parameters.dds" };
            auto& bufferUploads = *GetBufferUploads();
            for (unsigned c=0; c<3; ++c) {
                if (mainTargets._gbufferTextures[c]) {
                    auto stagingTexture = bufferUploads.Transaction_Immediate(stagingDesc[c], nullptr)->AdoptUnderlying();
                    context->GetUnderlying()->CopyResource(stagingTexture.get(), mainTargets._gbufferTextures[c].get());
                    D3DX11SaveTextureToFile(context->GetUnderlying(), stagingTexture.get(), D3DX11_IFF_DDS, outputNames[c]);
                }
            }
        }
    #endif


    void Deferred_DrawDebugging(RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, MainTargetsBox& mainTargets)
    {
        using namespace RenderCore;
        TRY {
            context->BindPS(MakeResourceList(5, mainTargets._gbufferRTVsSRV[0], mainTargets._gbufferRTVsSRV[1], mainTargets._gbufferRTVsSRV[2], mainTargets._msaaDepthBufferSRV));
            const bool useMsaaSamplers = mainTargets._desc._sampling._sampleCount > 1;
            auto& debuggingShader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2D.vsh:fullscreen:vs_*", 
                "game/xleres/deferred/debugging.psh:GBufferDebugging:ps_*",
                useMsaaSamplers?"MSAA_SAMPLERS=1":"");
            context->Bind(debuggingShader);
            context->Bind(Techniques::CommonResources()._blendStraightAlpha);
            SetupVertexGeneratorShader(context);
            context->Draw(4);
        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH_END

        context->UnbindPS<RenderCore::Metal::ShaderResourceView>(5, 4);
    }

}


