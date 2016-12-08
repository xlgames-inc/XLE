// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AmbientOcclusion.h"
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "SceneParser.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/ObjectFactory.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/RenderUtils.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/PtrUtils.h"
#include "../Math/Matrix.h"
#include "../Math/Transformations.h"
#include "../Math/ProjectionMath.h"
#include <algorithm>

#include "../RenderCore/DX11/Metal/DX11Utils.h"
#include "../RenderCore/DX11/Metal/Format.h"


    //
    //      Enabling nvidia AO --
    //
    //          The HBAO implementation from nvidia's Gameworks can be used here.
    //          I recommend using this AO implementation -- it should give a good
    //          result in most cases.
    //          However, the nvidia libraries aren't redistributed with this engine,
    //          so you'll need to manually enable yourself.
    //
    //          To enable, here's what you need to do:
    //              1) Download latest version from:
    //                      https://developer.nvidia.com/shadowworks
    //              2) Extract into:
    //                      xle/ForeignNonDist/GFSDK/HBAO
    //              3) In AmbientOcclusion.cpp, set the define:
    //                      "AO_IMPLEMENTATION" to "AO_IMPLEMENTATION_GFSDK"
    //              4) copy the dlls to the /Finals_* directories
    //
    //          If you use the nvidia Gameworks libraries for any projects, you must 
    //          abide by the licensing rules set by nvidia. Refer to nvidia's website
    //          for more information.
    //          

#define AO_IMPLEMENTATION_NONE      0
#define AO_IMPLEMENTATION_GFSDK     1

#if (defined(USER_djewsbury) || defined(USER_David)) && (GFXAPI_ACTIVE == GFXAPI_DX11)
    #define AO_IMPLEMENTATION       AO_IMPLEMENTATION_GFSDK
#else
    #define AO_IMPLEMENTATION       AO_IMPLEMENTATION_NONE
#endif

#if AO_IMPLEMENTATION == AO_IMPLEMENTATION_GFSDK

    #include <../../ForeignNonDist/GFSDK/HBAO/lib/GFSDK_SSAO.h>

        //  There's a problem here... The directory paths are always relative
        //  to the project directory of the final executable. But different
        //  executables may have different paths relative to the "ForeignNonDist" folder
    #if defined(_WIN64)
        #pragma comment(lib, "../../../ForeignNonDist/GFSDK/HBAO/lib/GFSDK_SSAO.win64.lib")
    #else
        #pragma comment(lib, "../../../ForeignNonDist/GFSDK/HBAO/lib/GFSDK_SSAO.win32.lib")
    #endif

#endif

#if AO_IMPLEMENTATION == AO_IMPLEMENTATION_GFSDK

namespace SceneEngine
{
    using namespace RenderCore;

    static GFSDK_SSAO_Parameters_D3D11 BuildAOParameters()
    {
        GFSDK_SSAO_Parameters_D3D11 parameters;
        parameters.Radius = 3.f;
        parameters.Bias = 0.1f;
        parameters.DetailAO = .5f;
        parameters.CoarseAO = 1.f;
        parameters.PowerExponent = 2.f;
        parameters.DepthStorage = GFSDK_SSAO_FP16_VIEW_DEPTHS;

        parameters.DepthThreshold.Enable = true;
        parameters.DepthThreshold.MaxViewDepth = 500.f;
        parameters.DepthThreshold.Sharpness = 100.f;

        parameters.Blur.Enable = true;
        parameters.Blur.Radius = GFSDK_SSAO_BLUR_RADIUS_8;
        parameters.Blur.Sharpness = 8.f;
        parameters.Blur.SharpnessProfile = GFSDK_SSAO_BlurSharpnessProfile();

        parameters.Output.BlendMode = GFSDK_SSAO_OVERWRITE_RGB;
        parameters.Output.CustomBlendState = GFSDK_SSAO_CustomBlendState_D3D11();
        parameters.Output.MSAAMode = GFSDK_SSAO_PER_PIXEL_AO;
        return parameters;
    }

    AmbientOcclusionResources::AmbientOcclusionResources(const Desc& desc)
    {
        GFSDK_SSAO_CustomHeap customHeap;
        #pragma push_macro("new")
        #undef new
            customHeap.new_ = ::operator new;
            customHeap.delete_ = ::operator delete;
        #pragma pop_macro("new")

        GFSDK_SSAO_Status status;
        GFSDK_SSAO_Context_D3D11* tempPtr = nullptr;
        status = GFSDK_SSAO_CreateContext_D3D11(Metal::GetObjectFactory().GetUnderlying(), &tempPtr, &customHeap);
        if (status != GFSDK_SSAO_OK || !tempPtr) {
            Throw(RenderCore::Exceptions::GenericFailure("Failure initializing GFSDK_SSAO"));
        }
        std::unique_ptr<GFSDK_SSAO_Context_D3D11, ContextDeletor> aoContext(tempPtr);

        GFSDK_SSAO_Version version;
        GFSDK_SSAO_GetVersion(&version);
        LogInfo << "GFSDK AO initialized with version: " << version.Major << "." << version.Minor << "." << version.Branch << "." << version.Revision;

        auto params = BuildAOParameters();
        status = aoContext->PreCreateRTs(&params, desc._width, desc._height);
        if (status != GFSDK_SSAO_OK) {
            Throw(RenderCore::Exceptions::GenericFailure("Failure while pre-creating AO RTs"));
        }

            // note -- always writing to non-MSAA texture
        auto& uploads = GetBufferUploads();
        auto bufferUploadsDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource|BindFlag::RenderTarget,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, desc._destinationFormat),
            "AOTarget");

        auto aoTexture = uploads.Transaction_Immediate(bufferUploadsDesc);
        Metal::RenderTargetView aoTarget(aoTexture->GetUnderlying());
        Metal::ShaderResourceView aoSRV(aoTexture->GetUnderlying());

        intrusive_ptr<BufferUploads::ResourceLocator> resolvedNormals;
        Metal::ShaderResourceView resolvedNormalsSRV;
        if (desc._useNormals && desc._normalsResolveFormat != Format::Unknown) {
            auto bufferUploadsDesc = BuildRenderTargetDesc(
                BindFlag::ShaderResource,
                BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, desc._normalsResolveFormat),
                "AONormalResolve");

            resolvedNormals = uploads.Transaction_Immediate(bufferUploadsDesc, nullptr);
            resolvedNormalsSRV = Metal::ShaderResourceView(resolvedNormals->GetUnderlying());
        }

        _aoTexture = std::move(aoTexture);
        _aoTarget = std::move(aoTarget);
        _aoSRV = std::move(aoSRV);
        _useNormals = desc._useNormals;
        _normalsResolveFormat = desc._normalsResolveFormat;
        _resolvedNormals = std::move(resolvedNormals);
        _resolvedNormalsSRV = std::move(resolvedNormalsSRV);
        _aoContext = std::move(aoContext);
    }
        
    AmbientOcclusionResources::~AmbientOcclusionResources()
    {
    }

    void AmbientOcclusionResources::ContextDeletor::operator()(void* ptr)
    {
            //  Destroying this object required a special case
            //  path. We don't call the destructor, or delete
            //  the pointer. We just call the Release virtual
            //  method (presumably because the object was allocated
            //  with a custom heap operator new, and we want
            //  to balance that with a custom delete)
        if (ptr) {
            ((GFSDK_SSAO_Context_D3D11*)ptr)->Release();
        }
    }

    static void AmbientOcclusion_DrawDebugging( Metal::DeviceContext& context,
                                                AmbientOcclusionResources& resources);


    void AmbientOcclusion_Render(   Metal::DeviceContext* context,
                                    LightingParserContext& parserContext,
                                    AmbientOcclusionResources& resources,
                                    const Metal::ShaderResourceView& depthBuffer,
                                    const Metal::ShaderResourceView* normalsBuffer,
                                    const Metal::ViewportDesc& mainViewport)
    {
            // Not working for orthogonal projection matrices
        if (IsOrthogonalProjection(parserContext.GetProjectionDesc()._cameraToProjection))
            return;

        static float SceneScale = 0.1f;

            //
            //      See nvidia header on documentation for interface to NVSSAO
            //      Note that MSAA behaviour is a little strange. the nvidia library
            //      will take a MSAA render target as input, but will write out
            //      non-MSAA data. So we can't blend directly to a MSAA buffer (
            //      blending a non-MSAA buffer with a MSAA buffer will remove the
            //      samples information!)
            //
        GFSDK_SSAO_InputData_D3D11 inputData;
        auto projectionMatrixTranspose = parserContext.GetProjectionDesc()._cameraToProjection;
        inputData.DepthData.DepthTextureType = GFSDK_SSAO_HARDWARE_DEPTHS;
        inputData.DepthData.ProjectionMatrix.Data = GFSDK_SSAO_Float4x4((const float*)&projectionMatrixTranspose);
        inputData.DepthData.ProjectionMatrix.Layout = GFSDK_SSAO_COLUMN_MAJOR_ORDER;
        inputData.DepthData.pFullResDepthTextureSRV = depthBuffer.GetUnderlying();
        inputData.DepthData.MetersToViewSpaceUnits = SceneScale;
        inputData.DepthData.Viewport.Enable = true;
        inputData.DepthData.Viewport.TopLeftX = (GFSDK_SSAO_UINT)mainViewport.TopLeftX;
        inputData.DepthData.Viewport.TopLeftY = (GFSDK_SSAO_UINT)mainViewport.TopLeftY;
        inputData.DepthData.Viewport.Width = (GFSDK_SSAO_UINT)mainViewport.Width;
        inputData.DepthData.Viewport.Height = (GFSDK_SSAO_UINT)mainViewport.Height;
        inputData.DepthData.Viewport.MinDepth = mainViewport.MinDepth;
        inputData.DepthData.Viewport.MaxDepth = mainViewport.MaxDepth;

        if (resources._useNormals && normalsBuffer) {
            if (resources._normalsResolveFormat != Format::Unknown) {
                context->GetUnderlying()->ResolveSubresource(
                    Metal::UnderlyingResourcePtr(resources._resolvedNormals->GetUnderlying()).get(), 0,
                    Metal::ExtractResource<ID3D::Resource>(normalsBuffer->GetUnderlying()).get(), 0,
                    Metal::AsDXGIFormat(resources._normalsResolveFormat));
                inputData.NormalData.pFullResNormalTextureSRV = resources._resolvedNormalsSRV.GetUnderlying();
            } else {
                inputData.NormalData.pFullResNormalTextureSRV = normalsBuffer->GetUnderlying();
            }
        
            //  when using UNORM normal data, use:
            // inputData.NormalData.DecodeScale =  2.f;
            // inputData.NormalData.DecodeBias  = -1.f;
            assert(GetComponentType(
                Metal::AsFormat(Metal::TextureDesc2D(normalsBuffer->GetUnderlying()).Format))
                == FormatComponentType::SNorm);
            inputData.NormalData.DecodeScale =  1.f;
            inputData.NormalData.DecodeBias  = 0.f;

            auto worldToView = InvertOrthonormalTransform(parserContext.GetProjectionDesc()._cameraToWorld);
            worldToView(2, 0) = -worldToView(2, 0);
            worldToView(2, 1) = -worldToView(2, 1);
            worldToView(2, 2) = -worldToView(2, 2);
            worldToView(2, 3) = -worldToView(2, 3);
            inputData.NormalData.WorldToViewMatrix.Data = GFSDK_SSAO_Float4x4((float*)&worldToView);
            inputData.NormalData.WorldToViewMatrix.Layout = GFSDK_SSAO_COLUMN_MAJOR_ORDER;
            inputData.NormalData.Enable = true;
        }

        context->InvalidateCachedState();   // (nvidia code might change some states)

            // Getting a warning message here if the pixel shader used
            // immediately before this point uses class instances. Seems to
            // be ok if we unbind the pixel shader first.
        context->Unbind<RenderCore::Metal::PixelShader>();
        context->Unbind<RenderCore::Metal::VertexShader>();
        context->Unbind<RenderCore::Metal::GeometryShader>();

        auto parameters = BuildAOParameters();
        auto status = resources._aoContext->RenderAO(
            context->GetUnderlying(), 
            &inputData, &parameters,
            resources._aoTarget.GetUnderlying());
        assert(status == GFSDK_SSAO_OK); (void)status;

        if (Tweakable("AODebugging", false)) {
            parserContext._pendingOverlays.push_back(
                std::bind(&AmbientOcclusion_DrawDebugging, std::placeholders::_1, std::ref(resources)));
        }
    }

    static void AmbientOcclusion_DrawDebugging(
        Metal::DeviceContext& context, AmbientOcclusionResources& resources)
    {
        SetupVertexGeneratorShader(context);
        context.BindPS(MakeResourceList(resources._aoSRV));
        context.Bind(Techniques::CommonResources()._blendStraightAlpha);
        context.Bind(::Assets::GetAssetDep<Metal::ShaderProgram>(
            "xleres/basic2D.vsh:fullscreen:vs_*", "xleres/postprocess/debugging.psh:AODebugging:ps_*"));
        context.Draw(4);
    }

}

#else

class GFSDK_SSAO_Context_D3D11 {};

namespace SceneEngine
{
    void AmbientOcclusionResources::ContextDeletor::operator()(void* ptr) {}

    AmbientOcclusionResources::AmbientOcclusionResources(const Desc& desc) {}
    AmbientOcclusionResources::~AmbientOcclusionResources() {}

    using namespace RenderCore::Metal;
    void AmbientOcclusion_Render(
        DeviceContext*,
        LightingParserContext&, AmbientOcclusionResources&,
        const ShaderResourceView&, const ShaderResourceView*,
        const ViewportDesc&)
    {
    }
}

#endif



