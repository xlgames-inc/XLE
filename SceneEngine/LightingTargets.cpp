// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LightingTargets.h"
#include "SceneEngineUtils.h"
#include "MetalStubs.h"

#include "../BufferUploads/ResourceLocator.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Resource.h"
#include "../Assets/AssetUtils.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/Console.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/StringFormat.h"

#include "../Foreign/DirectXTex/DirectXTex/DirectXTex.h"

namespace SceneEngine
{
#if 0
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

        ResourcePtr gbufferTextures[s_gbufferTextureCount];
        Metal::RenderTargetView gbufferRTV[dimof(gbufferTextures)];
        Metal::ShaderResourceView gbufferSRV[dimof(gbufferTextures)];
        std::fill(gbufferTextures, &gbufferTextures[dimof(gbufferTextures)], nullptr);

        auto bufferUploadsDesc = CreateDesc(
            BindFlag::ShaderResource|BindFlag::RenderTarget,
            0, GPUAccess::Write | GPUAccess::Read,
            BufferUploads::TextureDesc::Plain2D(
                desc._width, desc._height, Format::Unknown, 1, 0, desc._sampling),
            "GBuffer");
        for (unsigned c=0; c<dimof(gbufferTextures); ++c) {
            if (desc._gbufferFormats[c]._resourceFormat != Format::Unknown) {
                bufferUploadsDesc._textureDesc._format = desc._gbufferFormats[c]._resourceFormat;
                gbufferTextures[c] = CreateResourceImmediate(bufferUploadsDesc);
                gbufferRTV[c] = Metal::RenderTargetView(gbufferTextures[c]->GetUnderlying(), desc._gbufferFormats[c]._writeFormat);
                gbufferSRV[c] = Metal::ShaderResourceView(gbufferTextures[c]->GetUnderlying(), desc._gbufferFormats[c]._shaderReadFormat);
            }
        }

            /////////
        
        auto depthBufferDesc = CreateDesc(
            BindFlag::ShaderResource|BindFlag::DepthStencil,
            0, GPUAccess::Write | GPUAccess::Read,
            BufferUploads::TextureDesc::Plain2D(
                desc._width, desc._height, desc._depthFormat._resourceFormat, 1, 0, desc._sampling),
            "MainDepth");
        auto msaaDepthBufferTexture = CreateResourceImmediate(depthBufferDesc);
        auto secondaryDepthBufferTexture = CreateResourceImmediate(depthBufferDesc);
        Metal::DepthStencilView msaaDepthBuffer(msaaDepthBufferTexture->GetUnderlying(), desc._depthFormat._writeFormat);
        Metal::DepthStencilView secondaryDepthBuffer(secondaryDepthBufferTexture->GetUnderlying(), desc._depthFormat._writeFormat);
        Metal::ShaderResourceView msaaDepthBufferSRV(msaaDepthBufferTexture->GetUnderlying(), desc._depthFormat._shaderReadFormat);
        Metal::ShaderResourceView secondaryDepthBufferSRV(secondaryDepthBufferTexture->GetUnderlying(), desc._depthFormat._shaderReadFormat);

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
        auto bufferUploadsDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource|BindFlag::DepthStencil,
            BufferUploads::TextureDesc::Plain2D(
                desc._width, desc._height, desc._depthFormat._resourceFormat, 1, 0, desc._sampling),
            "ForwardTarget");

        auto msaaDepthBufferTexture = CreateResourceImmediate(bufferUploadsDesc);
        auto secondaryDepthBufferTexture = CreateResourceImmediate(bufferUploadsDesc);

            /////////

        Metal::DepthStencilView msaaDepthBuffer(msaaDepthBufferTexture->GetUnderlying(), desc._depthFormat._writeFormat);
        Metal::DepthStencilView secondaryDepthBuffer(secondaryDepthBufferTexture->GetUnderlying(), desc._depthFormat._writeFormat);

        Metal::ShaderResourceView msaaDepthBufferSRV(msaaDepthBufferTexture->GetUnderlying(), desc._depthFormat._shaderReadFormat);
        Metal::ShaderResourceView secondaryDepthBufferSRV(secondaryDepthBufferTexture->GetUnderlying(), desc._depthFormat._shaderReadFormat);

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
        auto bufferUploadsDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource|BindFlag::RenderTarget,
            BufferUploads::TextureDesc::Plain2D(
                desc._width, desc._height, desc._lightingResolveFormat._resourceFormat, 1, 0, 
                desc._sampling),
            "LightResolve");

        auto lightingResolveTexture = CreateResourceImmediate(bufferUploadsDesc);
        auto lightingResolveCopy = CreateResourceImmediate(bufferUploadsDesc);
        bufferUploadsDesc._textureDesc._samples = TextureSamples::Create();

        Metal::RenderTargetView lightingResolveTarget(lightingResolveTexture->GetUnderlying(), desc._lightingResolveFormat._writeFormat);
        Metal::ShaderResourceView lightingResolveSRV(lightingResolveTexture->GetUnderlying(), desc._lightingResolveFormat._shaderReadFormat);
        Metal::ShaderResourceView lightingResolveCopySRV(lightingResolveCopy->GetUnderlying(), desc._lightingResolveFormat._shaderReadFormat);

        _lightingResolveTexture = std::move(lightingResolveTexture);
        _lightingResolveRTV = std::move(lightingResolveTarget);
        _lightingResolveSRV = std::move(lightingResolveSRV);

        _lightingResolveCopy = std::move(lightingResolveCopy);
        _lightingResolveCopySRV = std::move(lightingResolveCopySRV);
    }

    LightingResolveTextureBox::~LightingResolveTextureBox()
    {
    }
#endif

    ///////////////////////////////////////////////////////////////////////////////////////////////

    unsigned LightingResolveShaders::LightShaderType::ReservedIndexCount()
    {
        return 0x1FF + 1;
    }

    unsigned LightingResolveShaders::LightShaderType::AsIndex() const
    {
            // We must compress the information in this object down
            // into a single unique id. We want to make sure each configuration
            // produces a unique id. But ids must be (close to) contiguous, and we want to
            // reserve as few id numbers as possible.
        auto shadows = _shadows;
        if (_shape == Sphere && shadows == OrthShadows) { shadows = PerspectiveShadows; }
        if (_shape == Tube || _shape == Rectangle || _shape == Disc) { shadows = NoShadows; }
        auto shadowResolveModel = _shadowResolveModel;
        if (shadows == NoShadows) { shadowResolveModel = 0; }

        return 
              ((_shape & 0x7) << 0)
            | ((shadows & 0x7) << 3)
            | ((_diffuseModel & 0x1) << 6)
            | ((shadowResolveModel & 0x1) << 7)
            | (unsigned(_hasScreenSpaceAO) << 8)
            ;
    }

    const char* AsLightResolverInterface(const LightingResolveShaders::LightShaderType& type)
    {
        switch (type._shape) {
        default:
        case LightingResolveShaders::Directional:   return "Directional";
        case LightingResolveShaders::Sphere:        return "Sphere";
        case LightingResolveShaders::Tube:          return "Tube";
        case LightingResolveShaders::Rectangle:     return "Rectangle";
        case LightingResolveShaders::Disc:          return "Disc";
        }
    }

    const char* AsShadowResolverInterface(const LightingResolveShaders::LightShaderType& type)
    {
        if (type._shadows == LightingResolveShaders::NoShadows)
            return "ShadowResolver_None";

        switch (type._shadowResolveModel) {
        default:
        case 0: return "ShadowResolver_PoissonDisc";
        case 1: return "ShadowResolver_Smooth";
        }
    }

    const char* AsCascadeResolverInterface(const LightingResolveShaders::LightShaderType& type)
    {
        switch (type._shadows) {
        default:
        case LightingResolveShaders::NoShadows:                 return "CascadeResolver_None";
        case LightingResolveShaders::PerspectiveShadows:        return "CascadeResolver_Arbitrary";
        case LightingResolveShaders::OrthHybridShadows:
        case LightingResolveShaders::OrthShadows:               return "CascadeResolver_Orthogonal";
        case LightingResolveShaders::OrthShadowsNearCascade:    return "CascadeResolver_OrthogonalWithNear";
        }
    }

    void LightingResolveShaders::BuildShader(const Desc& desc, const LightShaderType& type)
    {
        using namespace RenderCore;

        StringMeld<256, ::Assets::ResChar> definesTable;
        definesTable << "GBUFFER_TYPE=" << desc._gbufferType;
        definesTable << ";MSAA_SAMPLES=" << ((desc._msaaSampleCount<=1)?0:desc._msaaSampleCount);
        if (desc._msaaSamplers) definesTable << ";MSAA_SAMPLERS=1";

        if (desc._dynamicLinking==2) {
            definesTable << ";LIGHT_RESOLVE_DYN_LINKING=1";
        } else if (desc._dynamicLinking == 1) {
            definesTable << ";shape=" << AsLightResolverInterface(type);
            definesTable << ";cascade=" << AsCascadeResolverInterface(type);
            definesTable << ";shadows=" << AsShadowResolverInterface(type);
            if (desc._msaaSampleCount > 1)
                definesTable << ";passSampleIndex=true";
        } else {
            if (type._shadows != NoShadows) {
                definesTable << ";SHADOW_CASCADE_MODE=" << ((type._shadows == OrthShadows || type._shadows == OrthShadowsNearCascade || type._shadows == OrthHybridShadows) ? 2u : 1u);
                definesTable << ";SHADOW_ENABLE_NEAR_CASCADE=" << (type._shadows == OrthShadowsNearCascade ? 1u : 0u);
                definesTable << ";SHADOW_RESOLVE_MODEL=" << unsigned(type._shadowResolveModel);
                definesTable << ";SHADOW_RT_HYBRID=" << unsigned(type._shadows == OrthHybridShadows);
            }
            definesTable << ";LIGHT_SHAPE=" << unsigned(type._shape);
        }

        definesTable << ";DIFFUSE_METHOD=" << unsigned(type._diffuseModel);
        definesTable << ";HAS_SCREENSPACE_AO=" << unsigned(type._hasScreenSpaceAO);

        const char* vertexShader_viewFrustumVector = 
            desc._flipDirection
                ? "xleres/basic2D.vsh:fullscreen_flip_viewfrustumvector:vs_*"
                : "xleres/basic2D.vsh:fullscreen_viewfrustumvector:vs_*"
                ;

        LightShader& dest = _shaders[type.AsIndex()];
        assert(!dest._shader);
        dest._hasBeenResolved = false;

        if (desc._debugging) {
            dest._shader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
                vertexShader_viewFrustumVector, 
                "xleres/deferred/debugging/resolvedebug.psh:main:ps_*",
                definesTable.get());
        } else if (desc._dynamicLinking==1) {
            dest._shader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
                vertexShader_viewFrustumVector, 
                "xleres/deferred/resolvelightgraph.psh:main:ps_*",
                definesTable.get());
        } else if (desc._dynamicLinking==2) {
            dest._shader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
                vertexShader_viewFrustumVector, 
                "xleres/deferred/resolvelight.psh:main:!ps_*",
                definesTable.get());
        } else {
            dest._shader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
                vertexShader_viewFrustumVector, 
                (!desc._debugging)
                    ? "xleres/deferred/resolvelight.psh:main:ps_*"
                    : "xleres/deferred/debugging/resolvedebug.psh:main:ps_*",
                definesTable.get());
        }

        if (dest._shader)
            ::Assets::RegisterAssetDependency(_validationCallback, dest._shader->GetDependencyValidation());
    }

    auto LightingResolveShaders::GetShader(const LightShaderType& type) -> const LightShader*
    {
        auto index = type.AsIndex();
        if (index < _shaders.size()) {
            auto& dest = _shaders[index];

            // attempt to resolve this shader...
            if (dest._shader && !dest._hasBeenResolved) {
                using namespace RenderCore;

				UniformsStreamInterface usi;
				usi.BindConstantBuffer(CB::ShadowProj_Arbit, {Hash64("ArbitraryShadowProjection")});
                usi.BindConstantBuffer(CB::LightBuffer, {Hash64("LightBuffer")});
                usi.BindConstantBuffer(CB::ShadowParam, {Hash64("ShadowParameters")});
                usi.BindConstantBuffer(CB::ScreenToShadow, {Hash64("ScreenToShadowProjection")});
                usi.BindConstantBuffer(CB::ShadowProj_Ortho, {Hash64("OrthogonalShadowProjection")});
                usi.BindConstantBuffer(CB::ShadowResolveParam, {Hash64("ShadowResolveParameters")});
                usi.BindConstantBuffer(CB::ScreenToRTShadow, {Hash64("ScreenToRTShadowProjection")});
                usi.BindConstantBuffer(CB::Debugging, {Hash64("DebuggingGlobals")});
                usi.BindShaderResource(SR::DMShadow, {Hash64("ShadowTextures")});
                usi.BindShaderResource(SR::RTShadow_ListHead, Hash64("RTSListsHead"));
                usi.BindShaderResource(SR::RTShadow_LinkedLists, Hash64("RTSLinkedLists"));
                usi.BindShaderResource(SR::RTShadow_Triangles, Hash64("RTSTriangles"));

				dest._uniforms = Metal::BoundUniforms(
					*dest._shader, Metal::PipelineLayoutConfig{}, 
					Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
					usi);

                if (_dynamicLinking==2) {
                    dest._boundClassInterfaces = Metal::BoundClassInterfaces(*dest._shader);
                    dest._boundClassInterfaces.Bind(Hash64("MainResolver"), 0, AsLightResolverInterface(type));
                    dest._boundClassInterfaces.Bind(Hash64("MainCascadeResolver"), 0, AsCascadeResolverInterface(type));
                    dest._boundClassInterfaces.Bind(Hash64("MainShadowResolver"), 0, AsShadowResolverInterface(type));
                    dest._dynamicLinking = true;
                }
                dest._hasBeenResolved = true;
            }

            return &dest;
        }
        return nullptr; 
    }

    LightingResolveShaders::LightingResolveShaders(const Desc& desc)
    {
        using namespace RenderCore;
        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        _shaders.resize(LightShaderType::ReservedIndexCount());
        _dynamicLinking = desc._dynamicLinking;

            // find every sensible configuration, and build a new shader
            // and bound uniforms
        BuildShader(desc, LightShaderType(Directional, NoShadows, 0, 0, false));
        BuildShader(desc, LightShaderType(Directional, NoShadows, 1, 0, false));
        BuildShader(desc, LightShaderType(Directional, PerspectiveShadows, 0, 0, false));
        BuildShader(desc, LightShaderType(Directional, PerspectiveShadows, 1, 0, false));
        BuildShader(desc, LightShaderType(Directional, PerspectiveShadows, 0, 1, false));
        BuildShader(desc, LightShaderType(Directional, PerspectiveShadows, 1, 1, false));
        BuildShader(desc, LightShaderType(Directional, OrthShadows, 0, 0, false));
        BuildShader(desc, LightShaderType(Directional, OrthShadows, 1, 0, false));
        BuildShader(desc, LightShaderType(Directional, OrthShadows, 0, 1, false));
        BuildShader(desc, LightShaderType(Directional, OrthShadows, 1, 1, false));
        BuildShader(desc, LightShaderType(Directional, OrthShadowsNearCascade, 0, 0, false));
        BuildShader(desc, LightShaderType(Directional, OrthShadowsNearCascade, 1, 0, false));
        BuildShader(desc, LightShaderType(Directional, OrthShadowsNearCascade, 0, 1, false));
        BuildShader(desc, LightShaderType(Directional, OrthShadowsNearCascade, 1, 1, false));
        BuildShader(desc, LightShaderType(Directional, OrthHybridShadows, 0, 0, false));
        BuildShader(desc, LightShaderType(Directional, OrthHybridShadows, 1, 0, false));
        BuildShader(desc, LightShaderType(Directional, OrthHybridShadows, 0, 1, false));
        BuildShader(desc, LightShaderType(Directional, OrthHybridShadows, 1, 1, false));

        BuildShader(desc, LightShaderType(Directional, NoShadows, 0, 0, true));
        BuildShader(desc, LightShaderType(Directional, NoShadows, 1, 0, true));
        BuildShader(desc, LightShaderType(Directional, PerspectiveShadows, 0, 0, true));
        BuildShader(desc, LightShaderType(Directional, PerspectiveShadows, 1, 0, true));
        BuildShader(desc, LightShaderType(Directional, PerspectiveShadows, 0, 1, true));
        BuildShader(desc, LightShaderType(Directional, PerspectiveShadows, 1, 1, true));
        BuildShader(desc, LightShaderType(Directional, OrthShadows, 0, 0, true));
        BuildShader(desc, LightShaderType(Directional, OrthShadows, 1, 0, true));
        BuildShader(desc, LightShaderType(Directional, OrthShadows, 0, 1, true));
        BuildShader(desc, LightShaderType(Directional, OrthShadows, 1, 1, true));
        BuildShader(desc, LightShaderType(Directional, OrthShadowsNearCascade, 0, 0, true));
        BuildShader(desc, LightShaderType(Directional, OrthShadowsNearCascade, 1, 0, true));
        BuildShader(desc, LightShaderType(Directional, OrthShadowsNearCascade, 0, 1, true));
        BuildShader(desc, LightShaderType(Directional, OrthShadowsNearCascade, 1, 1, true));
        BuildShader(desc, LightShaderType(Directional, OrthHybridShadows, 0, 0, true));
        BuildShader(desc, LightShaderType(Directional, OrthHybridShadows, 1, 0, true));
        BuildShader(desc, LightShaderType(Directional, OrthHybridShadows, 0, 1, true));
        BuildShader(desc, LightShaderType(Directional, OrthHybridShadows, 1, 1, true));
        
        BuildShader(desc, LightShaderType(Sphere, NoShadows, 0, 0, false));
        BuildShader(desc, LightShaderType(Sphere, NoShadows, 1, 0, false));
        BuildShader(desc, LightShaderType(Sphere, PerspectiveShadows, 0, 0, false));
        BuildShader(desc, LightShaderType(Sphere, PerspectiveShadows, 1, 0, false));
        BuildShader(desc, LightShaderType(Sphere, PerspectiveShadows, 0, 1, false));
        BuildShader(desc, LightShaderType(Sphere, PerspectiveShadows, 1, 1, false));
        
        BuildShader(desc, LightShaderType(Sphere, NoShadows, 0, 0, true));
        BuildShader(desc, LightShaderType(Sphere, NoShadows, 1, 0, true));
        BuildShader(desc, LightShaderType(Sphere, PerspectiveShadows, 0, 0, true));
        BuildShader(desc, LightShaderType(Sphere, PerspectiveShadows, 1, 0, true));
        BuildShader(desc, LightShaderType(Sphere, PerspectiveShadows, 0, 1, true));
        BuildShader(desc, LightShaderType(Sphere, PerspectiveShadows, 1, 1, true));
        
        BuildShader(desc, LightShaderType(Tube, NoShadows, 0, 0, false));
        BuildShader(desc, LightShaderType(Tube, NoShadows, 1, 0, false));
        BuildShader(desc, LightShaderType(Tube, NoShadows, 0, 0, true));
        BuildShader(desc, LightShaderType(Tube, NoShadows, 1, 0, true));
        
        BuildShader(desc, LightShaderType(Rectangle, NoShadows, 0, 0, false));
        BuildShader(desc, LightShaderType(Rectangle, NoShadows, 1, 0, false));
        BuildShader(desc, LightShaderType(Rectangle, NoShadows, 0, 0, true));
        BuildShader(desc, LightShaderType(Rectangle, NoShadows, 1, 0, true));
        
        BuildShader(desc, LightShaderType(Disc, NoShadows, 0, 0, false));
        BuildShader(desc, LightShaderType(Disc, NoShadows, 1, 0, false));
        BuildShader(desc, LightShaderType(Disc, NoShadows, 0, 0, true));
        BuildShader(desc, LightShaderType(Disc, NoShadows, 1, 0, true));
    }

    LightingResolveShaders::~LightingResolveShaders() {}

    ///////////////////////////////////////////////////////////////////////////////////////////////

    AmbientResolveShaders::AmbientResolveShaders(const Desc& desc)
    {
        using namespace RenderCore;
        StringMeld<256> definesTable;

        definesTable 
            << "GBUFFER_TYPE=" << desc._gbufferType
            << ";MSAA_SAMPLES=" << ((desc._msaaSampleCount<=1)?0:desc._msaaSampleCount)
            << ";SKY_PROJECTION=" << desc._skyProjectionType
            << ";HAS_SCREENSPACE_AO=" << desc._hasAO
            << ";CALCULATE_TILED_LIGHTS=" << desc._hasTiledLighting
            << ";CALCULATE_SCREENSPACE_REFLECTIONS=" << desc._hasSRR
            << ";RESOLVE_RANGE_FOG=" << desc._rangeFog
            << ";HAS_DIFFUSE_IBL=" << (desc._hasIBL?1:0)
            << ";HAS_SPECULAR_IBL=" << (desc._hasIBL?1:0)
            ;

        if (desc._msaaSamplers)
            definesTable << ";MSAA_SAMPLERS=1";

        if (desc._referenceShaders)
            definesTable << ";REF_IBL=1";

        const char* vertexShader_viewFrustumVector = 
            desc._flipDirection
                ? "xleres/basic2D.vsh:fullscreen_flip_viewfrustumvector:vs_*"
                : "xleres/basic2D.vsh:fullscreen_viewfrustumvector:vs_*"
                ;

        auto* ambientLight = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            vertexShader_viewFrustumVector, 
            "xleres/deferred/resolveambient.psh:ResolveAmbient:ps_*",
            definesTable.get());

		UniformsStreamInterface usi;
		usi.BindConstantBuffer(0, {Hash64("AmbientLightBuffer")});
		auto ambientLightUniforms = std::make_unique<Metal::BoundUniforms>(
			std::ref(*ambientLight), Metal::PipelineLayoutConfig{},
			Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
			usi);

        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(validationCallback, ambientLight->GetDependencyValidation());

        _ambientLight = std::move(ambientLight);
        _ambientLightUniforms = std::move(ambientLightUniforms);
        _validationCallback = std::move(validationCallback);
    }

    AmbientResolveShaders::~AmbientResolveShaders() {}

    ///////////////////////////////////////////////////////////////////////////////////////////////


    #if defined(_DEBUG)
        void SaveGBuffer(RenderCore::Metal::DeviceContext& context, const MainTargets& mainTargets)
        {
			const char* outputNames[] = { "gbuffer_diffuse.dds", "gbuffer_normals.dds", "gbuffer_parameters.dds" };
			uint64_t semantics[] = { 
				RenderCore::Techniques::AttachmentSemantics::GBufferDiffuse,
				RenderCore::Techniques::AttachmentSemantics::GBufferNormal,
				RenderCore::Techniques::AttachmentSemantics::GBufferParameter };

            auto& bufferUploads = GetBufferUploads();
            for (unsigned c=0; c<3; ++c) {
				auto srcDesc = mainTargets.GetResource(semantics[c])->GetDesc();
				RenderCore::ResourceDesc stagingDesc;
                stagingDesc._type = RenderCore::ResourceDesc::Type::Texture;
                stagingDesc._bindFlags = 0;
                stagingDesc._cpuAccess = RenderCore::CPUAccess::Read;
                stagingDesc._gpuAccess = 0;
                stagingDesc._allocationRules = 0;
                stagingDesc._textureDesc = srcDesc._textureDesc;

                auto stagingTexture = bufferUploads.Transaction_Immediate(stagingDesc)->AdoptUnderlying();
                RenderCore::Metal::Copy(context, *checked_cast<RenderCore::Metal::Resource*>(stagingTexture.get()), *checked_cast<RenderCore::Metal::Resource*>(mainTargets.GetResource(semantics[c]).get()));
				assert(0); // todo -- need a texture write function
                // D3DX11SaveTextureToFile(context.GetUnderlying(), stagingTexture.get(), D3DX11_IFF_DDS, outputNames[c]);
            }
        }
    #endif


    void Deferred_DrawDebugging(RenderCore::Metal::DeviceContext& context, RenderCore::Techniques::ParsingContext& parserContext, MainTargets& mainTargets, bool useMsaaSamplers, unsigned debuggingType)
    {
        using namespace RenderCore;

        const auto* ps = "xleres/deferred/debugging.psh:GBufferDebugging:ps_*";
        if (debuggingType == 2)
            ps = "xleres/deferred/debugging.psh:GenericDebugging:!ps_*";

        StringMeld<256> meld;
        meld << useMsaaSamplers?"MSAA_SAMPLERS=1":"";
        bool enableParametersBuffer = Tweakable("EnableParametersBuffer", true);
        meld << ";GBUFFER_TYPE=" << enableParametersBuffer?1:2;
        
        auto& debuggingShader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
            "xleres/basic2D.vsh:fullscreen:vs_*", ps, meld.get());

        if (debuggingType == 2) {
            Metal::BoundClassInterfaces boundInterfaces(debuggingShader);
            auto loaders = Hash64("Loaders");
            boundInterfaces.Bind(loaders, 0, "Roughness");
            boundInterfaces.Bind(loaders, 1, "Specular");
            boundInterfaces.Bind(loaders, 2, "Metal");
            boundInterfaces.Bind(loaders, 3, "CookedAO");
            context.Bind(debuggingShader, boundInterfaces);
        } else {
            context.Bind(debuggingShader);
        }

        context.GetNumericUniforms(ShaderStage::Pixel).Bind(
			MakeResourceList(5, 
				mainTargets.GetSRV(Techniques::AttachmentSemantics::GBufferDiffuse), 
				mainTargets.GetSRV(Techniques::AttachmentSemantics::GBufferNormal), 
				mainTargets.GetSRV(Techniques::AttachmentSemantics::GBufferParameter), 
				mainTargets.GetSRV(Techniques::AttachmentSemantics::MultisampleDepth)));
        context.Bind(Techniques::CommonResources()._blendStraightAlpha);
        SetupVertexGeneratorShader(context);
        context.Draw(4);

		MetalStubs::UnbindPS<RenderCore::Metal::ShaderResourceView>(context, 5, 4);
    }

}


