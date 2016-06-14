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

#include "../RenderCore/Metal/TextureView.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/ResourceUtils.h"
#include "../RenderCore/IAnnotator.h"
#include "../Utility/StringFormat.h"
#include "../Utility/FunctionUtils.h"

#include "../RenderCore/DX11/Metal/IncludeDX11.h"

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

        ResLocator              _triangleBufferRes;
        Metal::VertexBuffer     _triangleBufferVB;
        SRV                     _triangleBufferSRV;

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

        auto& uploads = GetBufferUploads();
        auto indexFormat = (desc._indexDepth==16) ? Format::R16_UINT : Format::R32_UINT;
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

#if GFXAPI_ACTIVE == GFXAPI_DX11	// platformtemp
        _triangleBufferVB = Metal::VertexBuffer(_triangleBufferRes->ShareUnderlying());
        _triangleBufferSRV = SRV::RawBuffer(_triangleBufferRes->ShareUnderlying(), triangleSize*desc._triangleCount);
#endif

        _gridBufferUAV = UAV(_gridBuffer->ShareUnderlying());
        _gridBufferSRV = SRV(_gridBuffer->ShareUnderlying());
        _listsBufferUAV = UAV(
			_listsBuffer->ShareUnderlying(),
			TextureViewWindow(
				Format::Unknown, TextureDesc::Dimensionality::Undefined,
				TextureViewWindow::All,
				TextureViewWindow::All,
				TextureViewWindow::Flags::AttachedCounter));
        _listsBufferSRV = SRV(_listsBuffer->ShareUnderlying());

        _dummyTarget = uploads.Transaction_Immediate(
            CreateDesc(
                BindFlag::RenderTarget,
                0, GPUAccess::Read | GPUAccess::Write,
                BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Format::R8_UINT),
                "RTShadowsDummy"));
        _dummyRTV = RTV(_dummyTarget->ShareUnderlying());

        _gridBufferViewport = Metal::ViewportDesc { 0.f, 0.f, float(desc._width), float(desc._height), 0.f, 1.f };
    }

    RTShadowsBox::~RTShadowsBox() {}


    static const utf8* StringShadowCascadeMode = u("SHADOW_CASCADE_MODE");

    PreparedRTShadowFrustum PrepareRTShadows(
        IThreadContext& context,
        Metal::DeviceContext& metalContext, 
        LightingParserContext& parserContext,
        PreparedScene& preparedScene,
        const ShadowProjectionDesc& frustum,
        unsigned shadowFrustumIndex)
    {
        SceneParseSettings sceneParseSettings(
            SceneParseSettings::BatchFilter::RayTracedShadows, 
            ~SceneParseSettings::Toggles::BitField(0),
            shadowFrustumIndex);
        if (!parserContext.GetSceneParser()->HasContent(sceneParseSettings))
            return PreparedRTShadowFrustum();

        GPUAnnotation anno(context, "Prepare-RTShadows");

        auto& box = Techniques::FindCachedBox2<RTShadowsBox>(256, 256, 1024*1024, 32, 64*1024);
        auto oldSO = Metal::GeometryShader::GetDefaultStreamOutputInitializers();
        
        static const InputElementDesc soVertex[] = 
        {
            InputElementDesc("A", 0, Format::R32G32B32A32_FLOAT),
            InputElementDesc("B", 0, Format::R32G32_FLOAT),
            InputElementDesc("C", 0, Format::R32G32B32A32_FLOAT),
            InputElementDesc("D", 0, Format::R32G32B32_FLOAT)
        };

        static const InputElementDesc il[] = 
        {
            InputElementDesc("A", 0, Format::R32G32B32A32_FLOAT),
            InputElementDesc("B", 0, Format::R32G32_FLOAT),
            InputElementDesc("C", 0, Format::R32G32B32A32_FLOAT),
            InputElementDesc("D", 0, Format::R32G32B32_FLOAT)
        };

        metalContext.UnbindPS<Metal::ShaderResourceView>(5, 3);

        const unsigned bufferCount = 1;
        unsigned strides[] = { 52 };
        unsigned offsets[] = { 0 };
        Metal::GeometryShader::SetDefaultStreamOutputInitializers(
            Metal::GeometryShader::StreamOutputInitializers(soVertex, dimof(soVertex), strides, 1));

        static_assert(bufferCount == dimof(strides), "Stream output buffer count mismatch");
        static_assert(bufferCount == dimof(offsets), "Stream output buffer count mismatch");
        metalContext.BindSO(MakeResourceList(box._triangleBufferVB));

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
            StringShadowCascadeMode, 
            (preparedResult._mode == ShadowProjectionDesc::Projections::Mode::Ortho)?2:1);

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
        Float4x4 savedWorldToProjection = parserContext.GetProjectionDesc()._worldToProjection;
        parserContext.GetProjectionDesc()._worldToProjection = frustum._worldToClip;
        auto cleanup = MakeAutoCleanup(
            [&parserContext, &savedWorldToProjection]() {
                parserContext.GetProjectionDesc()._worldToProjection = savedWorldToProjection;
                parserContext.GetTechniqueContext()._runtimeState.SetParameter(StringShadowCascadeMode, 0);
            });

        CATCH_ASSETS_BEGIN
            parserContext.GetSceneParser()->ExecuteScene(
                context, parserContext, sceneParseSettings, 
                preparedScene, TechniqueIndex_RTShadowGen);
        CATCH_ASSETS_END(parserContext)

        metalContext.UnbindSO();
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

            Metal::BoundInputLayout inputLayout(InputLayout(il, dimof(il)), shader);
            metalContext.Bind(inputLayout);

                // no shader constants/resources required

            metalContext.ClearUInt(box._gridBufferUAV, { 0, 0, 0, 0 });

            metalContext.Bind(Techniques::CommonResources()._blendOpaque);
            metalContext.Bind(Techniques::CommonResources()._dssDisable);
            metalContext.Bind(Techniques::CommonResources()._cullDisable);
            metalContext.Bind(Topology::PointList);
            metalContext.Bind(MakeResourceList(box._triangleBufferVB), strides[0], offsets[0]);

            metalContext.Bind(
                MakeResourceList(box._dummyRTV), nullptr,
                MakeResourceList(box._gridBufferUAV, box._listsBufferUAV));
            metalContext.DrawAuto();
        CATCH_ASSETS_END(parserContext)

        metalContext.Bind(Topology::TriangleList);
        savedTargets.ResetToOldTargets(metalContext);

        preparedResult._listHeadSRV = box._gridBufferSRV;
        preparedResult._linkedListsSRV = box._listsBufferSRV;
        preparedResult._trianglesSRV = box._triangleBufferSRV;
        return std::move(preparedResult);
    }


    void RTShadows_DrawMetrics(
        RenderCore::Metal::DeviceContext& context, 
        LightingParserContext& parserContext, IMainTargets& mainTargets)
    {
        SavedTargets savedTargets(context);
        auto restoreMarker = savedTargets.MakeResetMarker(context);

#if GFXAPI_ACTIVE == GFXAPI_DX11	// platformtemp
        context.GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr); // (unbind depth)
#endif

        context.BindPS(MakeResourceList(5, mainTargets.GetSRV(IMainTargets::GBufferDiffuse), mainTargets.GetSRV(IMainTargets::GBufferNormals), mainTargets.GetSRV(IMainTargets::GBufferParameters), mainTargets.GetSRV(IMainTargets::MultisampledDepth)));
        const bool useMsaaSamplers = mainTargets.GetSampling()._sampleCount > 1;

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

        context.Bind(debuggingShader);
        context.Bind(Techniques::CommonResources()._blendStraightAlpha);
        SetupVertexGeneratorShader(context);

        for (const auto& p:parserContext._preparedRTShadows) {
            const Metal::ShaderResourceView* srvs[] = 
                { &p.second._listHeadSRV, &p.second._linkedListsSRV, &p.second._trianglesSRV, &mainTargets.GetSRV(IMainTargets::MultisampledDepth) };

            SharedPkt constants[2];
            const Metal::ConstantBuffer* prebuiltConstants[2] = {nullptr, nullptr};
            prebuiltConstants[0] = &p.second._orthoCB;
            constants[1] = BuildScreenToShadowConstants(
                p.second, parserContext.GetProjectionDesc()._cameraToWorld,
                parserContext.GetProjectionDesc()._cameraToProjection);

            uniforms.Apply(
                context, parserContext.GetGlobalUniformsStream(), 
                Metal::UniformsStream(constants, prebuiltConstants, dimof(constants), srvs, dimof(srvs)));
        }

        context.Draw(4);
    }

}