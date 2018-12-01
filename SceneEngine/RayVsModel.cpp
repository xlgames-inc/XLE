// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RayVsModel.h"
#include "SceneEngineUtils.h"
#include "LightingParser.h"
#include "LightingParserContext.h"
#include "MetalStubs.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/Types.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../ConsoleRig/ResourceBox.h"

namespace SceneEngine
{
    using namespace RenderCore;

    class ModelIntersectionStateContext::Pimpl
    {
    public:
        std::shared_ptr<IThreadContext> _threadContext;
        ModelIntersectionResources* _res;
        MetalStubs::GeometryShader::StreamOutputInitializers _oldSO;

        RenderCore::Techniques::ParsingContext _parserContext;

        Pimpl(const Techniques::TechniqueContext& techniqueContext)
            : _parserContext(techniqueContext) {}
    };

    class ModelIntersectionResources
    {
    public:
        class Desc 
        {
        public:
            unsigned _elementSize;
            unsigned _elementCount;
            Desc(unsigned elementSize, unsigned elementCount) : _elementSize(elementSize), _elementCount(elementCount) {}
        };
        using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;

        ResLocator _streamOutputBuffer;
        ResLocator _clearedBuffer;
        ResLocator _cpuAccessBuffer;
        ModelIntersectionResources(const Desc&);
    };

    ModelIntersectionResources::ModelIntersectionResources(const Desc& desc)
    {
        auto& uploads = SceneEngine::GetBufferUploads();

        LinearBufferDesc lbDesc;
        lbDesc._structureByteSize = desc._elementSize;
        lbDesc._sizeInBytes = desc._elementSize * desc._elementCount;

        auto bufferDesc = CreateDesc(
            BindFlag::StreamOutput | BindFlag::TransferDst, 0, GPUAccess::Read | GPUAccess::Write,
            lbDesc, "ModelIntersectionBuffer");
        
        _streamOutputBuffer = uploads.Transaction_Immediate(bufferDesc);

        _cpuAccessBuffer = uploads.Transaction_Immediate(
            CreateDesc(0, CPUAccess::Read, 0, lbDesc, "ModelIntersectionCopyBuffer"));

        auto pkt = BufferUploads::CreateEmptyPacket(bufferDesc);
        XlSetMemory(pkt->GetData(), 0, pkt->GetDataSize());
        _clearedBuffer = uploads.Transaction_Immediate(
            CreateDesc(
                BindFlag::StreamOutput | BindFlag::TransferSrc, 0, GPUAccess::Read | GPUAccess::Write,
                lbDesc, "ModelIntersectionClearingBuffer"), 
            pkt.get());
    }

    auto ModelIntersectionStateContext::GetResults() -> std::vector<ResultEntry>
    {
        std::vector<ResultEntry> result;

        auto metalContext = Metal::DeviceContext::Get(*_pimpl->_threadContext);

            // We must lock the stream output buffer, and look for results within it
            // It seems that this kind of thing wasn't part of the original intentions
            // for stream output. So the results can appear anywhere within the buffer.
            // We have to search for non-zero entries. Results that haven't been written
            // to will appear zeroed out.
        Metal::Copy(
            *metalContext,
            Metal::AsResource(*_pimpl->_res->_cpuAccessBuffer->GetUnderlying()), 
            Metal::AsResource(*_pimpl->_res->_streamOutputBuffer->GetUnderlying()));

        using namespace BufferUploads;
        auto& uploads = SceneEngine::GetBufferUploads();
        auto readback = uploads.Resource_ReadBack(*_pimpl->_res->_cpuAccessBuffer);
        if (readback && readback->GetData()) {
            const auto* mappedData = (const ResultEntry*)readback->GetData();
            unsigned resultCount = 0;
            for (unsigned c=0; c<s_maxResultCount; ++c) {
                if (mappedData[c]._depthAsInt) { ++resultCount; }
            }
            result.reserve(resultCount);
            for (unsigned c=0; c<s_maxResultCount; ++c) {
                if (mappedData[c]._depthAsInt) { 
                    result.push_back(mappedData[c]);
                }
            }
            std::sort(result.begin(), result.end(), &ResultEntry::CompareDepth);
        }

        // D3D11_MAPPED_SUBRESOURCE mappedSub;
        // auto hresult = metalContext->GetUnderlying()->Map(
        //     _pimpl->_res->_cpuAccessBuffer.get(), 0, D3D11_MAP_READ, 0, &mappedSub);
        // if (SUCCEEDED(hresult)) {
        // 
        //     const auto* mappedData = (const ResultEntry*)mappedSub.pData;
        //     unsigned resultCount = 0;
        //     for (unsigned c=0; c<s_maxResultCount; ++c) {
        //         if (mappedData[c]._depthAsInt) { ++resultCount; }
        //     }
        //     result.reserve(resultCount);
        //     for (unsigned c=0; c<s_maxResultCount; ++c) {
        //         if (mappedData[c]._depthAsInt) { 
        //             result.push_back(mappedData[c]);
        //         }
        //     }
        // 
        //     metalContext->GetUnderlying()->Unmap(_pimpl->_res->_cpuAccessBuffer.get(), 0);
        // 
        //     std::sort(result.begin(), result.end(), &ResultEntry::CompareDepth);
        // }

        return std::move(result);
    }

    void ModelIntersectionStateContext::SetRay(const std::pair<Float3, Float3> worldSpaceRay)
    {
        float rayLength = Magnitude(worldSpaceRay.second - worldSpaceRay.first);
        struct RayDefinitionCBuffer
        {
            Float3 _rayStart;
            float _rayLength;
            Float3 _rayDirection;
            unsigned _dummy;
        } rayDefinitionCBuffer = 
        {
            worldSpaceRay.first, rayLength,
            (worldSpaceRay.second - worldSpaceRay.first) / rayLength, 0
        };

        auto metalContext = Metal::DeviceContext::Get(*_pimpl->_threadContext);
        metalContext->GetNumericUniforms(ShaderStage::Geometry).Bind(
            MakeResourceList(
                8, MakeMetalCB(&rayDefinitionCBuffer, sizeof(rayDefinitionCBuffer))));
    }

    void ModelIntersectionStateContext::SetFrustum(const Float4x4& frustum)
    {
        struct FrustumDefinitionCBuffer
        {
            Float4x4 _frustum;
        } frustumDefinitionCBuffer = { frustum };

        auto metalContext = Metal::DeviceContext::Get(*_pimpl->_threadContext);
        metalContext->GetNumericUniforms(ShaderStage::Geometry).Bind(
            MakeResourceList(
                9, MakeMetalCB(&frustumDefinitionCBuffer, sizeof(frustumDefinitionCBuffer))));
    }

    RenderCore::Techniques::ParsingContext& ModelIntersectionStateContext::GetParserContext()
    {
        return _pimpl->_parserContext;
    }

    ModelIntersectionStateContext::ModelIntersectionStateContext(
        TestType testType,
        std::shared_ptr<IThreadContext> threadContext,
        const Techniques::TechniqueContext& techniqueContext,
        const Techniques::CameraDesc* cameraForLOD)
    {
        _pimpl = std::make_unique<Pimpl>(techniqueContext);
        _pimpl->_threadContext = threadContext;

        _pimpl->_parserContext.GetSubframeShaderSelectors().SetParameter(
            (const utf8*)"INTERSECTION_TEST", unsigned(testType));

        auto metalContext = Metal::DeviceContext::Get(*threadContext);

            // We're doing the intersection test in the geometry shader. This means
            // we have to setup a projection transform to avoid removing any potential
            // intersection results during screen-edge clipping.
            // Also, if we want to know the triangle pts and barycentric coordinates,
            // we need to make sure that no clipping occurs.
            // The easiest way to prevent clipping would be use a projection matrix that
            // would transform all points into a single point in the center of the view
            // frustum.
        Metal::ViewportDesc newViewport(0.f, 0.f, float(255.f), float(255.f), 0.f, 1.f);
        metalContext->Bind(newViewport);

		RenderingQualitySettings qualitySettings{UInt2(256, 256)};

        Float4x4 specialProjMatrix = MakeFloat4x4(
            0.f, 0.f, 0.f, 0.5f,
            0.f, 0.f, 0.f, 0.5f,
            0.f, 0.f, 0.f, 0.5f,
            0.f, 0.f, 0.f, 1.f);

            // The camera settings can affect the LOD that objects a rendered with.
            // So, in some cases we need to initialise the camera to the same state
            // used in rendering. This will ensure that we get the right LOD behaviour.
        Techniques::CameraDesc camera;
        if (cameraForLOD) { camera = *cameraForLOD; }

        LightingParser_SetupScene(*threadContext, _pimpl->_parserContext);
        LightingParser_SetGlobalTransform(
            *threadContext, _pimpl->_parserContext, 
            BuildProjectionDesc(camera, qualitySettings._dimensions, &specialProjMatrix));

        _pimpl->_oldSO = MetalStubs::GeometryShader::GetDefaultStreamOutputInitializers();

        static const InputElementDesc eles[] = {
            InputElementDesc("INTERSECTIONDEPTH",   0, Format::R32_FLOAT),
            InputElementDesc("POINT",               0, Format::R32G32B32A32_FLOAT),
            InputElementDesc("POINT",               1, Format::R32G32B32A32_FLOAT),
            InputElementDesc("POINT",               2, Format::R32G32B32A32_FLOAT),
            InputElementDesc("DRAWCALLINDEX",       0, Format::R32_UINT),
            InputElementDesc("MATERIALGUID",        0, Format::R32G32_UINT)
        };

        static const unsigned strides[] = { sizeof(ResultEntry) };
        MetalStubs::GeometryShader::SetDefaultStreamOutputInitializers(
            MetalStubs::GeometryShader::StreamOutputInitializers(
                eles, dimof(eles), strides, dimof(strides)));

        _pimpl->_res = &ConsoleRig::FindCachedBox<ModelIntersectionResources>(
            ModelIntersectionResources::Desc(sizeof(ResultEntry), s_maxResultCount));

            // the only way to clear these things is copy from another buffer...
        Metal::Copy(
            *metalContext,
            Metal::AsResource(*_pimpl->_res->_streamOutputBuffer->GetUnderlying()),
            Metal::AsResource(*_pimpl->_res->_clearedBuffer->GetUnderlying()));

        auto soBuffer = _pimpl->_res->_streamOutputBuffer->GetUnderlying();
        MetalStubs::BindSO(*metalContext, MakeResourceList(soBuffer));

        auto& commonRes = Techniques::CommonResources();
        metalContext->GetNumericUniforms(ShaderStage::Geometry).Bind(MakeResourceList(commonRes._defaultSampler));
    }

    ModelIntersectionStateContext::~ModelIntersectionStateContext()
    {
        auto metalContext = Metal::DeviceContext::Get(*_pimpl->_threadContext);
        MetalStubs::UnbindSO(*metalContext);
        MetalStubs::GeometryShader::SetDefaultStreamOutputInitializers(_pimpl->_oldSO);
    }
}




