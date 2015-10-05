// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RayTracedShadows.h"
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "SceneParser.h"
#include "SceneEngineUtils.h"
#include "LightDesc.h"
#include "LightInternal.h"
#include "LightingTargets.h"        // for MainTargetsBox in RTShadows_DrawMetrics

#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/ResourceLocator.h"

#include "../RenderCore/Metal/RenderTargetView.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/Metal/GPUProfiler.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Resource.h"
#include "../Utility/StringFormat.h"

#include "../RenderCore/DX11/Metal/DX11Utils.h"

namespace SceneEngine
{
    using namespace RenderCore;

    class RTShadowsBox
    {
    public:
        class Desc
        {
        public:
            unsigned _width, _height;
            unsigned _storageCount;
            unsigned _indexDepth;   // 16 or 32
            unsigned _triangleCount;

            Desc(unsigned width, unsigned height, unsigned storageCount, unsigned indexDepth, unsigned triangleCount)
                : _width(width), _height(height), _storageCount(storageCount), _indexDepth(indexDepth), _triangleCount(triangleCount) {}
        };

        using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;
        using RTV = RenderCore::Metal::RenderTargetView;
        using UAV = RenderCore::Metal::UnorderedAccessView;
        using SRV = RenderCore::Metal::ShaderResourceView;

        UAV _gridBufferUAV;
        SRV _gridBufferSRV;
        UAV _listsBufferUAV;
        SRV _listsBufferSRV;
        ResLocator _gridBuffer;
        ResLocator _listsBuffer;

        RTV _dummyRTV;
        ResLocator _dummyTarget;

        intrusive_ptr<ID3D::Buffer> _triangleBuffer;
        ResLocator _triangleBufferRes;
        Metal::VertexBuffer _triangleBufferVB;
        SRV _triangleBufferSRV;

        Metal::ViewportDesc _gridBufferViewport;

        RTShadowsBox(const Desc&);
        ~RTShadowsBox();
    };

    RTShadowsBox::RTShadowsBox(const Desc& desc)
    {
        // we need 2 main buffers:
        //  1. "grid buffer" -- for each cell in the screen space grid, this has the starting
        //          point of the linked list of triangles
        //  2. "lists buffer" -- this contains all of the linked lists; interleaved together

        using namespace BufferUploads;
        using namespace Metal::NativeFormat;

        auto& uploads = GetBufferUploads();
        auto indexFormat = (desc._indexDepth==16) ? R16_UINT : R32_UINT;
        unsigned indexSize = (desc._indexDepth==16) ? 2 : 4;

        _gridBuffer = uploads.Transaction_Immediate(
            CreateDesc(
                BindFlag::UnorderedAccess | BindFlag::ShaderResource,
                0, GPUAccess::Read | GPUAccess::Write,
                BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, indexFormat),
                "RTShadowsGrid"));

        _listsBuffer = uploads.Transaction_Immediate(
            CreateDesc(
                BindFlag::UnorderedAccess | BindFlag::ShaderResource | BindFlag::StructuredBuffer,
                0, GPUAccess::Read | GPUAccess::Write,
                BufferUploads::LinearBufferDesc::Create(indexSize*2*desc._storageCount, indexSize*2),
                "RTShadowsList"));

            // note that if we want to use alpha test textures on the triangles,
            // we will need to leave space for the texture coordinates as well.
        unsigned triangleSize = 52;
        _triangleBufferRes = uploads.Transaction_Immediate(
            CreateDesc(
                BindFlag::StreamOutput | BindFlag::ShaderResource | BindFlag::VertexBuffer | BindFlag::RawViews,
                0, GPUAccess::Read | GPUAccess::Write,
                BufferUploads::LinearBufferDesc::Create(triangleSize*desc._triangleCount, triangleSize),
                "RTShadowsTriangles"));

        _triangleBuffer = Metal::QueryInterfaceCast<ID3D::Buffer>(_triangleBufferRes->GetUnderlying());
        _triangleBufferVB = Metal::VertexBuffer(_triangleBufferRes->GetUnderlying());
        _triangleBufferSRV = SRV::RawBuffer(_triangleBufferRes->GetUnderlying(), triangleSize*desc._triangleCount);

        _gridBufferUAV = UAV(_gridBuffer->GetUnderlying());
        _gridBufferSRV = SRV(_gridBuffer->GetUnderlying());
        _listsBufferUAV = UAV(_listsBuffer->GetUnderlying(), UAV::Flags::AttachedCounter);
        _listsBufferSRV = SRV(_listsBuffer->GetUnderlying());

        _dummyTarget = uploads.Transaction_Immediate(
            CreateDesc(
                BindFlag::RenderTarget,
                0, GPUAccess::Read | GPUAccess::Write,
                BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, R8_UINT),
                "RTShadowsDummy"));
        _dummyRTV = RTV(_dummyTarget->GetUnderlying());

        _gridBufferViewport = Metal::ViewportDesc { 0.f, 0.f, float(desc._width), float(desc._height), 0.f, 1.f };
    }

    RTShadowsBox::~RTShadowsBox() {}


    static const std::string StringShadowCascadeMode = "SHADOW_CASCADE_MODE";

    PreparedRTShadowFrustum PrepareRTShadows(
        Metal::DeviceContext& metalContext, 
        LightingParserContext& parserContext,
        const ShadowProjectionDesc& frustum,
        unsigned shadowFrustumIndex)
    {
        SceneParseSettings sceneParseSettings(
            SceneParseSettings::BatchFilter::RayTracedShadows, 
            ~SceneParseSettings::Toggles::BitField(0),
            shadowFrustumIndex);
        if (!parserContext.GetSceneParser()->HasContent(sceneParseSettings))
            return PreparedRTShadowFrustum();

        Metal::GPUProfiler::DebugAnnotation anno(metalContext, L"Prepare-RTShadows");

        auto& box = Techniques::FindCachedBox2<RTShadowsBox>(256, 256, 1024*1024, 32, 64*1024);
        auto oldSO = Metal::GeometryShader::GetDefaultStreamOutputInitializers();
        
        static const Metal::InputElementDesc soVertex[] = 
        {
            Metal::InputElementDesc("A", 0, Metal::NativeFormat::R32G32B32A32_FLOAT),
            Metal::InputElementDesc("B", 0, Metal::NativeFormat::R32G32_FLOAT),
            Metal::InputElementDesc("C", 0, Metal::NativeFormat::R32G32B32A32_FLOAT),
            Metal::InputElementDesc("D", 0, Metal::NativeFormat::R32G32B32_FLOAT)
        };

        static const Metal::InputElementDesc il[] = 
        {
            Metal::InputElementDesc("A", 0, Metal::NativeFormat::R32G32B32A32_FLOAT),
            Metal::InputElementDesc("B", 0, Metal::NativeFormat::R32G32_FLOAT),
            Metal::InputElementDesc("C", 0, Metal::NativeFormat::R32G32B32A32_FLOAT),
            Metal::InputElementDesc("D", 0, Metal::NativeFormat::R32G32B32_FLOAT)
        };

        metalContext.UnbindPS<Metal::ShaderResourceView>(5, 3);

        const unsigned bufferCount = 1;
        unsigned strides[] = { 52 };
        unsigned offsets[] = { 0 };
        Metal::GeometryShader::SetDefaultStreamOutputInitializers(
            Metal::GeometryShader::StreamOutputInitializers(soVertex, dimof(soVertex), strides, 1));

        ID3D::Buffer* targets[] = { box._triangleBuffer.get() };
        static_assert(bufferCount == dimof(strides), "Stream output buffer count mismatch");
        static_assert(bufferCount == dimof(offsets), "Stream output buffer count mismatch");
        static_assert(bufferCount == dimof(targets), "Stream output buffer count mismatch");
        metalContext.GetUnderlying()->SOSetTargets(bufferCount, targets, offsets);

            // set up the render state for writing into the grid buffer
        SavedTargets savedTargets(metalContext);
        metalContext.Bind(box._gridBufferViewport);
        metalContext.Unbind<Metal::RenderTargetView>();
        metalContext.Bind(Techniques::CommonResources()._blendOpaque);
        metalContext.Bind(Techniques::CommonResources()._defaultRasterizer);    // for newer video cards, we need "conservative raster" enabled

        PreparedRTShadowFrustum preparedResult;
        preparedResult.InitialiseConstants(&metalContext, frustum._projections);
        using TC = Techniques::TechniqueContext;
        parserContext.SetGlobalCB(metalContext, TC::CB_ShadowProjection, &preparedResult._arbitraryCBSource, sizeof(preparedResult._arbitraryCBSource));
        parserContext.SetGlobalCB(metalContext, TC::CB_OrthoShadowProjection, &preparedResult._orthoCBSource, sizeof(preparedResult._orthoCBSource));

        parserContext.GetTechniqueContext()._runtimeState.SetParameter(
            (const utf8*)StringShadowCascadeMode.c_str(), 
            preparedResult._mode == ShadowProjectionDesc::Projections::Mode::Ortho?2:1);

            // Now, we need to transform the object's triangle buffer into shadow
            // projection space during this step (also applying skinning, wind bending, and
            // any other animation effects. 
            //
            // Each object that will be used with projected shadows must have a buffer 
            // containing the triangle information.
            //
            // We can deal with this in a number of ways:
            //      1. rtwritetiles shader writes triangles out in a stream-output step
            //      2. transform triangles first, then pass that information through the rtwritetiles shader
            //      3. transform triangles completely separately from the rtwritetiles step
            //
            // Method 1 would avoid extra transformations of the input data, and actually
            // simplifies some of the shader work. We don't need any special input buffers
            // or extra input data. The shaders just take generic model information, and build
            // everything they need, as they need it.
            //
            // We can also choose to reject backfacing triangles at this point, as well as 
            // removing triangles that are culled from the frustum.
            //
        CATCH_ASSETS_BEGIN
            auto savedWorldToProjection = parserContext.GetProjectionDesc()._worldToProjection;
            auto& projDesc = parserContext.GetProjectionDesc();
            projDesc._worldToProjection = frustum._worldToClip;

            parserContext.GetSceneParser()->ExecuteScene(
                &metalContext, parserContext, sceneParseSettings, 
                TechniqueIndex_RTShadowGen);

            projDesc._worldToProjection = savedWorldToProjection;
        CATCH_ASSETS_END(parserContext)

        metalContext.GetUnderlying()->SOSetTargets(0, nullptr, nullptr);
        Metal::GeometryShader::SetDefaultStreamOutputInitializers(oldSO);

            // We have the list of triangles. Let's render then into the final
            // grid buffer viewport. This should create a list of triangles for
            // each cell in the grid. The goal is to reduce the number of triangles
            // that the ray tracing shader needs to look at.
            //
            // We could attempt to do this in the same step above. But that creates
            // some problems with frustum cull and back face culling. This order
            // allows us reduce the total triangle count before we start assigning
            // primitive ids.
            //
            // todo -- also calculate min/max for each grid during this step
        CATCH_ASSETS_BEGIN
            auto& shader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/shadowgen/rtwritetiles.sh:vs_passthrough:vs_*",
                "game/xleres/shadowgen/consraster.sh:gs_conservativeRasterization:gs_*",
                "game/xleres/shadowgen/rtwritetiles.sh:ps_main:ps_*",
                "OUTPUT_PRIM_ID=1;INPUT_RAYTEST_TRIS=1");
            metalContext.Bind(shader);

            Metal::BoundInputLayout inputLayout(Metal::InputLayout(il, dimof(il)), shader);
            metalContext.Bind(inputLayout);

                // no shader constants/resources required

            unsigned clearValues[] = { 0, 0, 0, 0 };
            metalContext.Clear(box._gridBufferUAV, clearValues);

            metalContext.Bind(Techniques::CommonResources()._blendOpaque);
            metalContext.Bind(Techniques::CommonResources()._dssDisable);
            metalContext.Bind(Techniques::CommonResources()._cullDisable);
            metalContext.Bind(Metal::Topology::PointList);
            metalContext.Bind(MakeResourceList(box._triangleBufferVB), strides[0], offsets[0]);

            ID3D::RenderTargetView* rtv[] = { box._dummyRTV.GetUnderlying() };
            ID3D::UnorderedAccessView* uavs[] = { box._gridBufferUAV.GetUnderlying(), box._listsBufferUAV.GetUnderlying() };
            const UINT initialCounts[] = { UINT(0), UINT(0) };
            static_assert(dimof(initialCounts) == dimof(uavs), "Initial count array size mismatch");
            metalContext.GetUnderlying()->OMSetRenderTargetsAndUnorderedAccessViews(
                dimof(rtv), rtv, nullptr,
                dimof(rtv), dimof(uavs), uavs, initialCounts);

            metalContext.GetUnderlying()->DrawAuto();
        CATCH_ASSETS_END(parserContext)

        metalContext.Bind(Metal::Topology::TriangleList);
        savedTargets.ResetToOldTargets(metalContext);
        parserContext.GetTechniqueContext()._runtimeState.SetParameter((const utf8*)StringShadowCascadeMode.c_str(), 0);

        preparedResult._listHeadSRV = box._gridBufferSRV;
        preparedResult._linkedListsSRV = box._listsBufferSRV;
        preparedResult._trianglesSRV = box._triangleBufferSRV;
        return std::move(preparedResult);
    }


    void RTShadows_DrawMetrics(
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext, MainTargetsBox& mainTargets)
    {
        SavedTargets savedTargets(*context);

        CATCH_ASSETS_BEGIN
            context->GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr); // (unbind depth)

            context->BindPS(MakeResourceList(5, mainTargets._gbufferRTVsSRV[0], mainTargets._gbufferRTVsSRV[1], mainTargets._gbufferRTVsSRV[2], mainTargets._msaaDepthBufferSRV));
            const bool useMsaaSamplers = mainTargets._desc._sampling._sampleCount > 1;

            StringMeld<256> defines;
            defines << "SHADOW_CASCADE_MODE=2";
            if (useMsaaSamplers) defines << ";MSAA_SAMPLERS=1";

            auto& debuggingShader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2D.vsh:fullscreen:vs_*", 
                "game/xleres/shadowgen/rtshadmetrics.sh:ps_main:ps_*",
                defines.get());
            Metal::BoundUniforms uniforms(debuggingShader);
            Techniques::TechniqueContext::BindGlobalUniforms(uniforms);
            uniforms.BindShaderResources(1, {"RTSListsHead", "RTSLinkedLists", "RTSTriangles", "DepthTexture"});
            uniforms.BindConstantBuffers(1, {"OrthogonalShadowProjection", "ScreenToShadowProjection"});

            context->Bind(debuggingShader);
            context->Bind(Techniques::CommonResources()._blendStraightAlpha);
            SetupVertexGeneratorShader(*context);

            for (const auto& p:parserContext._preparedRTShadows) {
                const Metal::ShaderResourceView* srvs[] = 
                    { &p.second._listHeadSRV, &p.second._linkedListsSRV, &p.second._trianglesSRV, &mainTargets._msaaDepthBufferSRV };

                SharedPkt constants[2];
                const Metal::ConstantBuffer* prebuiltConstants[2] = {nullptr, nullptr};
                prebuiltConstants[0] = &p.second._orthoCB;
                constants[1] = BuildScreenToShadowConstants(
                    p.second, parserContext.GetProjectionDesc()._cameraToWorld);

                uniforms.Apply(
                    *context, parserContext.GetGlobalUniformsStream(), 
                    Metal::UniformsStream(constants, prebuiltConstants, dimof(constants), srvs, dimof(srvs)));
            }

            context->Draw(4);
        CATCH_ASSETS_END(parserContext)

        savedTargets.ResetToOldTargets(*context);
    }

}