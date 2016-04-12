// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TiledLighting.h"
#include "SceneEngineUtils.h"
#include "MetricsBox.h"
#include "LightingParserContext.h"
#include "SceneParser.h"
#include "LightDesc.h"

#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Assets/DeferredShaderResource.h"
#include "../RenderCore/Metal/TextureView.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/RenderUtils.h"
#include "../RenderCore/Format.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../Math/Matrix.h"
#include "../Math/Transformations.h"
#include "../ConsoleRig/Console.h"

#include "../Utility/StringFormat.h"

// #include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/DX11/Metal/DX11Utils.h"

namespace SceneEngine
{
    using namespace RenderCore;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    class TileLightingResources
    {
    public:
        class Desc
        {
        public:
            unsigned    _width, _height, _bitDepth;
            Desc(unsigned width, unsigned height, unsigned bitDepth) : _width(width), _height(height), _bitDepth(bitDepth) {}
        };

        Metal::UnorderedAccessView  _debuggingTexture[3];
        Metal::ShaderResourceView   _debuggingTextureSRV[3];

        Metal::UnorderedAccessView  _lightOutputTexture;
        Metal::UnorderedAccessView  _temporaryProjectedLights;
        Metal::ShaderResourceView   _lightOutputTextureSRV;

        TileLightingResources(const Desc& desc);
        ~TileLightingResources();
    };

    TileLightingResources::TileLightingResources(const Desc& desc)
    {
        auto& uploads = GetBufferUploads();
        auto resLocator0 = uploads.Transaction_Immediate(
            BuildRenderTargetDesc(
                BufferUploads::BindFlag::UnorderedAccess | BufferUploads::BindFlag::ShaderResource, 
                BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Format::R32_TYPELESS, 1),
                "TileLighting"));
        auto resLocator1 = uploads.Transaction_Immediate(
            BuildRenderTargetDesc(
                BufferUploads::BindFlag::UnorderedAccess | BufferUploads::BindFlag::ShaderResource, 
                BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Format::R32_TYPELESS, 1),
                "TileLighting"));
        auto resLocator2 = uploads.Transaction_Immediate(
            BuildRenderTargetDesc(
                BufferUploads::BindFlag::UnorderedAccess | BufferUploads::BindFlag::ShaderResource, 
                BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Format::R16_TYPELESS, 1),
                "TileLighting"));

        Metal::ShaderResourceView srv0(resLocator0->GetUnderlying(), Format::R32_FLOAT);
        Metal::ShaderResourceView srv1(resLocator1->GetUnderlying(), Format::R32_FLOAT);
        Metal::ShaderResourceView srv2(resLocator2->GetUnderlying(), Format::R16_UINT);

        Metal::UnorderedAccessView uav0(resLocator0->GetUnderlying(), Format::R32_UINT);
        Metal::UnorderedAccessView uav1(resLocator1->GetUnderlying(), Format::R32_UINT);
        Metal::UnorderedAccessView uav2(resLocator2->GetUnderlying(), Format::R16_UINT);

        // UINT clearValues[4] = { 0x3fffffff, 0x3fffffff, 0x3fffffff, 0x3fffffff };
        // auto device  = MainBridge::GetInstance()->GetDevice();
        // auto context = RenderCore::Metal::DeviceContext::GetImmediateContext(device);
        // context->Clear(uav0, clearValues);
        // context->Clear(uav1, clearValues);
        // context->Clear(uav2, clearValues);

            /////

        auto resLocator3 = uploads.Transaction_Immediate(
            BuildRenderTargetDesc(
                BufferUploads::BindFlag::UnorderedAccess | BufferUploads::BindFlag::ShaderResource, 
                BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, (desc._bitDepth==16)?Format::R16G16B16A16_FLOAT:Format::R32G32B32A32_FLOAT, 1),
                "TileLighting"),
            nullptr)->AdoptUnderlying();

        BufferUploads::BufferDesc bufferDesc;
        bufferDesc._type = BufferUploads::BufferDesc::Type::LinearBuffer;
        bufferDesc._bindFlags = BufferUploads::BindFlag::StructuredBuffer | BufferUploads::BindFlag::UnorderedAccess;
        bufferDesc._cpuAccess = 0;
        bufferDesc._gpuAccess = BufferUploads::GPUAccess::Read | BufferUploads::GPUAccess::Write;
        bufferDesc._allocationRules = 0;
        bufferDesc._linearBufferDesc._structureByteSize = 24;
        bufferDesc._linearBufferDesc._sizeInBytes = 1024 * bufferDesc._linearBufferDesc._structureByteSize;
        auto resLocator4 = uploads.Transaction_Immediate(bufferDesc, nullptr)->AdoptUnderlying();

        Metal::UnorderedAccessView lightOutputTexture(resLocator3.get());
        Metal::UnorderedAccessView temporaryProjectedLights(resLocator4.get());
        Metal::ShaderResourceView lightOutputTextureSRV(resLocator3.get());

        _lightOutputTexture = std::move(lightOutputTexture);
        _temporaryProjectedLights = std::move(temporaryProjectedLights);
        _lightOutputTextureSRV = std::move(lightOutputTextureSRV);
        _debuggingTexture[0] = std::move(uav0);
        _debuggingTexture[1] = std::move(uav1);
        _debuggingTexture[2] = std::move(uav2);
        _debuggingTextureSRV[0] = std::move(srv0);
        _debuggingTextureSRV[1] = std::move(srv1);
        _debuggingTextureSRV[2] = std::move(srv2);
    }

    TileLightingResources::~TileLightingResources()
    {}

    void TiledLighting_DrawDebugging(
        RenderCore::Metal::DeviceContext& context, 
        LightingParserContext& lightingParserContext,
        TileLightingResources& tileLightingResources)
    {
        context.BindPS(MakeResourceList(tileLightingResources._lightOutputTextureSRV));
        context.BindPS(MakeResourceList(1, tileLightingResources._debuggingTextureSRV[0], tileLightingResources._debuggingTextureSRV[1], tileLightingResources._debuggingTextureSRV[2]));
        context.BindPS(MakeResourceList(4, ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/DefaultResources/digits.dds:T").GetShaderResource()));
        auto& debuggingShader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", 
            "game/xleres/deferred/debugging.psh:DepthsDebuggingTexture:ps_*");
        context.Bind(debuggingShader);
        context.Bind(Techniques::CommonResources()._blendStraightAlpha);
        SetupVertexGeneratorShader(context);
        context.Draw(4);
        context.UnbindPS<Metal::ShaderResourceView>(0, 4);
    }

    RenderCore::Metal::ShaderResourceView TiledLighting_CalculateLighting(
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& lightingParserContext,
        Metal::ShaderResourceView& depthsSRV, Metal::ShaderResourceView& normalsSRV)
    {
        const bool doTiledRenderingTest             = Tweakable("DoTileRenderingTest", false);
        const bool doClusteredRenderingTest         = Tweakable("TileClustering", true);
        const bool tiledBeams                       = Tweakable("TiledBeams", false);

        const unsigned maxLightCount                = 1024;
        const unsigned tileLightCount               = std::min(Tweakable("TileLightCount", 512), int(maxLightCount));
        const bool pause                            = Tweakable("Pause", false);

        if (doTiledRenderingTest && !tiledBeams) {
            CATCH_ASSETS_BEGIN
                Metal::TextureDesc2D tDesc(depthsSRV.GetUnderlying());
                unsigned width = tDesc.Width, height = tDesc.Height, sampleCount = tDesc.SampleDesc.Count;

                auto& tileLightingResources = Techniques::FindCachedBox<TileLightingResources>(
                    TileLightingResources::Desc(width, height, 16));

                auto worldToView = InvertOrthonormalTransform(
                    lightingParserContext.GetProjectionDesc()._cameraToWorld);
                auto coordinateFlipMatrix = Float4x4(
                    1.f, 0.f, 0.f, 0.f,
                    0.f, 0.f, -1.f, 0.f,
                    0.f, 1.f, 0.f, 0.f,
                    0.f, 0.f, 0.f, 1.f);
                worldToView = Combine(worldToView, coordinateFlipMatrix);

                struct LightStruct
                {
                    Float3  _worldSpacePosition;
                    float   _radius;
                    Float3  _colour;
                    float   _power;

                    LightStruct(const Float3& worldSpacePosition, float radius, const Float3& colour, float power) 
                        : _worldSpacePosition(worldSpacePosition), _radius(radius), _colour(colour), _power(power) {}
                };

                static float startingAngle = 0.f;
                static Metal::ShaderResourceView lightBuffer;
                static intrusive_ptr<BufferUploads::ResourceLocator> lightBufferResource;
                if (!lightBufferResource) {
                    auto& uploads = GetBufferUploads();
                    BufferUploads::BufferDesc desc;
                    desc._type = BufferUploads::BufferDesc::Type::LinearBuffer;
                    desc._bindFlags = BufferUploads::BindFlag::ShaderResource | BufferUploads::BindFlag::StructuredBuffer;
                    desc._cpuAccess = BufferUploads::CPUAccess::WriteDynamic;
                    desc._gpuAccess = BufferUploads::GPUAccess::Read;
                    desc._allocationRules = 0;
                    desc._linearBufferDesc._sizeInBytes = sizeof(LightStruct) * (maxLightCount+1);
                    desc._linearBufferDesc._structureByteSize = sizeof(LightStruct);
                    lightBufferResource = uploads.Transaction_Immediate(desc, nullptr);

                    if (lightBufferResource) {
                        lightBuffer = Metal::ShaderResourceView(lightBufferResource->GetUnderlying());
                    }
                }

                {
                    static Float3 baseLightPosition = Float3(1600.f, 2400.f, 150.f);

                    std::vector<LightStruct> lights;
                    lights.reserve(tileLightCount+1);
                    for (unsigned c=0; c<tileLightCount; ++c) {
                        const float X = startingAngle + c / float(tileLightCount) * gPI * 2.f;
                        const float Y = 3.7397f * startingAngle + .7234f * c / float(tileLightCount) * gPI * 2.f;
                        const float Z = 13.8267f * startingAngle + 0.27234f * c / float(tileLightCount) * gPI * 2.f;
                        const float radius = 60.f + 20.f * XlSin(Z);
                        lights.push_back(LightStruct(
                            baseLightPosition + Float3(200.f * XlCos(X), 2.f * c, 80.f * XlSin(Y) * XlCos(Y)), 
                            radius, .25f * Float3(.65f + .35f * XlSin(Y), .65f + .35f * XlCos(Y), .65f + .35f * XlCos(X)),
                            PowerForHalfRadius(radius, 0.05f)));
                    }
                    if (!pause) {
                        startingAngle += 0.05f;
                    }

                        // add dummy light
                    lights.push_back(LightStruct(Float3(0.f, 0.f, 0.f), 0.f, Float3(0.f, 0.f, 0.f), 0.f));

                    D3D11_MAPPED_SUBRESOURCE mappedRes;
                    HRESULT hresult = context->GetUnderlying()->Map(
                        Metal::UnderlyingResourcePtr(lightBufferResource->GetUnderlying()).get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
                    if (SUCCEEDED(hresult)) {
                        if (mappedRes.pData) {
                            XlCopyMemory(mappedRes.pData, AsPointer(lights.cbegin()), lights.size() * sizeof(LightStruct));
                        }
                        context->GetUnderlying()->Unmap(
                            Metal::UnderlyingResourcePtr(lightBufferResource->GetUnderlying()).get(), 0);
                    }
                }

                auto& projDesc = lightingParserContext.GetProjectionDesc();
                Float2 fov;
                fov[1] = projDesc._verticalFov;
                fov[0] = 2.f * XlATan(projDesc._aspectRatio * XlTan(projDesc._verticalFov  * .5f));
                
                const unsigned TileWidth = 16, TileHeight = 16;
                struct LightCulling
                {
                    int         _lightCount;
                    int         _groupCounts[2];
                    unsigned    _dummy0;
                    Float4x4    _worldToView;
                    Float2      _fov;
                    int         _dummy1[2];
                } lightCulling = { 
                    tileLightCount, { (width + TileWidth - 1) / TileWidth, (height + TileHeight + 1) / TileHeight }, 0,
                    worldToView, 
                    fov, { 0, 0 }
                };
                Metal::ConstantBuffer cbuffer(&lightCulling, sizeof(lightCulling));
                context->BindCS(MakeResourceList(lightingParserContext.GetGlobalTransformCB(), Metal::ConstantBuffer(), cbuffer));

                context->Bind(ResourceList<Metal::RenderTargetView, 0>(), nullptr); // reading from depth buffer (so must clear it from output)

                context->BindCS(MakeResourceList(lightBuffer, depthsSRV, normalsSRV));
                context->BindCS(MakeResourceList(tileLightingResources._lightOutputTexture, tileLightingResources._temporaryProjectedLights));
                context->BindCS(MakeResourceList(4, 
                    lightingParserContext.GetMetricsBox()->_metricsBufferUAV, 
                    tileLightingResources._debuggingTexture[0], tileLightingResources._debuggingTexture[1], tileLightingResources._debuggingTexture[2]));

                context->Bind(::Assets::GetAssetDep<Metal::ComputeShader>("game/xleres/deferred/tiled.csh:PrepareLights"));
                context->Dispatch((tileLightCount + 256 - 1) / 256);
                        
                char definesTable[256];
                Utility::XlFormatString(definesTable, dimof(definesTable), "MSAA_SAMPLES=%i", sampleCount);
        
                if (doClusteredRenderingTest) {
                    context->Bind(::Assets::GetAssetDep<Metal::ComputeShader>("game/xleres/deferred/clustered.csh:main", definesTable));
                } else {
                    context->Bind(::Assets::GetAssetDep<Metal::ComputeShader>("game/xleres/deferred/tiled.csh:main", definesTable));
                }
                context->Dispatch(lightCulling._groupCounts[0], lightCulling._groupCounts[1]);

                    //
                    //      inject point light sources into fog
                    //          (currently too expensive to use practically)
                    //
                #if 0
                    if (!processedResult.empty() && processedResult[0]._projectConstantBuffer && doVolumetricFog && fogPointLightSources) {
                        auto& fogRes = FindCachedBox<VolumetricFogResources>(VolumetricFogResources::Desc(processedResult[0]._frustumCount));
                        auto& fogShaders = FindCachedBoxDep<VolumetricFogShaders>(VolumetricFogShaders::Desc(1, useMsaaSamplers, false));
                        auto& airLight = FindCachedBox<AirLightResources>(AirLightResources::Desc());
                        
                        context->BindCS(MakeResourceList(3, fogRes._inscatterPointLightsValuesUnorderedAccess));
                        context->BindCS(MakeResourceList(13, Assets::GetAssetDep<Metal::DeferredShaderResource>("game/xleres/DefaultResources/balanced_noise.dds:LT").GetShaderResource()));
                        context->BindCS(MakeResourceList(1, airLight._lookupShaderResource));
                        context->Bind(*fogShaders._injectPointLightSources);
                        context->Dispatch(160/10, 90/10, 128/8);
                    }
                #endif

                context->UnbindCS<Metal::UnorderedAccessView>(0, 8);
                context->UnbindCS<Metal::ShaderResourceView>(0, 3);
                context->Unbind<Metal::ComputeShader>();

                if (Tweakable("TiledLightingDebugging", false) && !tiledBeams) {
                    lightingParserContext._pendingOverlays.push_back(
                        std::bind(&TiledLighting_DrawDebugging, std::placeholders::_1, std::placeholders::_2, tileLightingResources));
                }

                return tileLightingResources._lightOutputTextureSRV;
            CATCH_ASSETS_END(lightingParserContext)
        }

        return Metal::ShaderResourceView();
    }

    Metal::ConstantBuffer DuplicateResource(Metal::DeviceContext* context, Metal::ConstantBuffer& inputResource)
    {
        return Metal::ConstantBuffer(
            ::RenderCore::Metal_DX11::DuplicateResource(context->GetUnderlying(), inputResource.GetUnderlying()));
    }

    void TiledLighting_RenderBeamsDebugging( RenderCore::Metal::DeviceContext* context, 
                                            LightingParserContext& lightingParserContext,
                                            bool active, unsigned mainViewportWidth, unsigned mainViewportHeight, 
                                            unsigned techniqueIndex)
    {
        static bool lastActive = false;
        if (active) {
            CATCH_ASSETS_BEGIN
                static Metal::ConstantBuffer savedGlobalTransform;
                if (lastActive != active) {
                    if (!lightingParserContext.GetGlobalTransformCB().GetUnderlying()) {
                        savedGlobalTransform = Metal::ConstantBuffer();
                        return;
                    }
                    savedGlobalTransform = DuplicateResource(context, lightingParserContext.GetGlobalTransformCB());
                }

                if (!savedGlobalTransform.GetUnderlying()) {
                    return;
                }

                auto& tileLightingResources = Techniques::FindCachedBox<TileLightingResources>(
                    TileLightingResources::Desc(mainViewportWidth, mainViewportHeight, 16));

                bool isShadowsPass = techniqueIndex == TechniqueIndex_ShadowGen;

                auto& debuggingShader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                    "game/xleres/deferred/debugging/beams.vsh:main:vs_*", 
                    "game/xleres/deferred/debugging/beams.gsh:main:gs_*", 
                    "game/xleres/deferred/debugging/beams.psh:main:ps_*",
                    isShadowsPass?"SHADOWS=1;SHADOW_CASCADE_MODE=1":"");    // hack -- SHADOW_CASCADE_MODE let explicitly here

                Metal::BoundUniforms uniforms(debuggingShader);
                uniforms.BindConstantBuffer(Hash64("RecordedTransform"), 0, 1);
                uniforms.BindConstantBuffer(Hash64("GlobalTransform"), 1, 1);
                uniforms.BindConstantBuffer(Hash64("$Globals"), 2, 1);
                const unsigned TileWidth = 16, TileHeight = 16;
                uint32 globals[4] = {   (mainViewportWidth + TileWidth - 1) / TileWidth, 
                                        (mainViewportHeight + TileHeight + 1) / TileHeight, 
                                        0, 0 };
                Metal::ConstantBufferPacket constants[]  = { Metal::ConstantBufferPacket(), Metal::ConstantBufferPacket(), MakeSharedPkt(globals) };
                const Metal::ConstantBuffer* prebuiltBuffers[]   = { &savedGlobalTransform, &lightingParserContext.GetGlobalTransformCB(), nullptr };
                uniforms.Apply(*context, lightingParserContext.GetGlobalUniformsStream(), Metal::UniformsStream(constants, prebuiltBuffers));
                                
                context->BindVS(MakeResourceList(tileLightingResources._debuggingTextureSRV[0], tileLightingResources._debuggingTextureSRV[1]));
                context->Bind(Techniques::CommonResources()._dssReadWrite);
                SetupVertexGeneratorShader(*context);
                context->Bind(Metal::Topology::PointList);

                if (!isShadowsPass && Tweakable("TiledBeamsTransparent", false)) {
                    context->Bind(Techniques::CommonResources()._blendStraightAlpha);
                    auto& predepth = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                        "game/xleres/deferred/debugging/beams.vsh:main:vs_*", 
                        "game/xleres/deferred/debugging/beams.gsh:main:gs_*", 
                        "game/xleres/deferred/debugging/beams.psh:predepth:ps_*",
                        "");
                    context->Bind(predepth);
                    context->Draw(globals[0]*globals[1]);
                } else {
                    context->Bind(Techniques::CommonResources()._blendOpaque);
                }

                context->Bind(debuggingShader);
                context->Draw(globals[0]*globals[1]);

                if (!isShadowsPass) {
                    context->Bind(::Assets::GetAssetDep<Metal::ShaderProgram>(
                        "game/xleres/deferred/debugging/beams.vsh:main:vs_*", 
                        "game/xleres/deferred/debugging/beams.gsh:Outlines:gs_*", 
                        "game/xleres/deferred/debugging/beams.psh:main:ps_*",
                        ""));
                    context->Draw(globals[0]*globals[1]);
                }

                context->UnbindVS<Metal::ShaderResourceView>(0, 2);
            CATCH_ASSETS_END(lightingParserContext)
        }

        lastActive = active;
    }


}