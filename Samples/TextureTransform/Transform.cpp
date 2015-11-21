// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Transform.h"
#include "MinimalAssetServices.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/ShaderService.h"
#include "../../RenderCore/Techniques/PredefinedCBLayout.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Metal/Format.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/ShaderResource.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../BufferUploads/ResourceLocator.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../Assets/Assets.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Conversion.h"
#include <map>

#include "../../Core/WinAPI/IncludeWindows.h"
#include "../../Foreign/DirectXTex/DirectXTex/DirectXTex.h"

namespace TextureTransform
{
    using namespace RenderCore;

    static CompiledShaderByteCode LoadShaderImmediate(const ::Assets::ResChar name[])
    {
        CompiledShaderByteCode compiledByteCode(name);
        auto state = compiledByteCode.StallWhilePending();
        if (state == ::Assets::AssetState::Invalid) {
            std::string errorString;
            auto errorBlob = compiledByteCode.GetErrors();
            if (errorBlob && !errorBlob->empty()) {
                errorString = std::string(
                    (const char*)AsPointer(errorBlob->begin()),
                    (const char*)AsPointer(errorBlob->end()));
            } else {
                errorString = "Failure while loading or compiling shader byte code";
            }
            Throw(::Assets::Exceptions::InvalidAsset(name, errorString.c_str()));
        }
        return std::move(compiledByteCode);
    }

    class InputResource
    {
    public:
        Metal::ShaderResourceView _srv;
        intrusive_ptr<BufferUploads::ResourceLocator> _resLocator;
        BufferUploads::TextureDesc _desc;
        Metal::NativeFormat::Enum _finalFormat;
        uint64 _bindingHash;

        InputResource(const ::Assets::ResChar initializer[]);
        ~InputResource();
    private:
        enum class SourceColorSpace { SRGB, Linear, Unspecified };
    };

    InputResource::InputResource(const ::Assets::ResChar initializer[])
    {
        _finalFormat = Metal::NativeFormat::Unknown;
        _desc = BufferUploads::TextureDesc::Plain1D(0, Metal::NativeFormat::Unknown);
        _bindingHash = 0;

        auto splitter = MakeFileNameSplitter(initializer);

        SourceColorSpace colSpace = SourceColorSpace::Unspecified;
        for (auto c:splitter.Parameters()) {
            if (c == 'l' || c == 'L') { colSpace = SourceColorSpace::Linear; }
            if (c == 's' || c == 'S') { colSpace = SourceColorSpace::SRGB; }
        }

        if (colSpace == SourceColorSpace::Unspecified)
            colSpace = XlFindStringI(initializer, "_ddn") ? SourceColorSpace::Linear : SourceColorSpace::SRGB;
        
        using namespace BufferUploads;
        auto& uploads = Samples::MinimalAssetServices::GetBufferUploads();
        auto inputPacket = CreateStreamingTextureSource(splitter.AllExceptParameters());
        _resLocator = uploads.Transaction_Immediate(
            CreateDesc(
                BindFlag::ShaderResource,
                0, GPUAccess::Read,
                TextureDesc::Empty(), "TextureProcessInput"),
            inputPacket.get());

        if (_resLocator) {
            _desc = ExtractDesc(*_resLocator->GetUnderlying())._textureDesc;

            auto format = (Metal::NativeFormat::Enum)_desc._nativePixelFormat;
            if (colSpace == SourceColorSpace::SRGB) format = Metal::AsSRGBFormat(format);
            else if (colSpace == SourceColorSpace::Linear) format = Metal::AsLinearFormat(format);
            _srv = Metal::ShaderResourceView(_resLocator->GetUnderlying(), format);
            _finalFormat = format;
        }
    }

    InputResource::~InputResource(){}

    static Metal::NativeFormat::Enum AsRTFormat(Metal::NativeFormat::Enum input)
    {
        // Convert the input format into a format that we can write to.
        // Normally this should mean just converting a compressed format into
        // a plain RGBA format.
        auto compression = Metal::GetCompressionType(input);
        if (compression == Metal::FormatCompressionType::BlockCompression) {
            return Metal::FindFormat(
                Metal::FormatCompressionType::None,
                Metal::GetComponents(input),
                Metal::GetComponentType(input),
                Metal::GetDecompressedComponentPrecision(input));
        }
        return input;
    }

    static ::Assets::rstring GetCBLayoutName(const ::Assets::ResChar input[])
    {
        auto splitter = MakeFileNameSplitter(input);
        return splitter.DriveAndPath().AsString()
            + splitter.File().AsString()
            + "_cblayout.txt";
    }

    TextureResult ExecuteTransform(
        IDevice& device,
        StringSection<char> xleDir,
        StringSection<char> shaderName,
        const ParameterBox& parameters,
        std::map<std::string, ProcessingFn> fns)
    {
        using namespace BufferUploads;

        std::vector<InputResource> inputResources;

            // We'll interpret every string in the input parameters as a resource name
        for (auto i=parameters.Begin(); !i.IsEnd(); ++i) {
            if (i.Type()._typeHint == ImpliedTyping::TypeHint::String) {
                auto value = ImpliedTyping::AsString(
                    i.RawValue(), ptrdiff_t(i.ValueTableEnd()) - ptrdiff_t(i.RawValue()), i.Type());

                InputResource inputRes(value.c_str());
                if (inputRes._srv.IsGood()) {
                    inputRes._bindingHash = Hash64((const char*)i.Name());
                    inputResources.push_back(std::move(inputRes));
                }
            }
        }

///////////////////////////////////////////////////////////////////////////////////////////////////
            // We need to calculate an output format and viewport dimensions
            // If we have at least one input resource, we can use that to select
            // an initial width/height/format.
            // Otherwise, let's look for parameters named "Dims" and "Format"

        auto rtFormat = Metal::NativeFormat::Unknown;
        UInt2 viewDims(0, 0);
        if (!inputResources.empty()) {
            rtFormat = inputResources[0]._finalFormat;
            viewDims = UInt2(inputResources[0]._desc._width, inputResources[0]._desc._height);
        }

        auto dimsParam = parameters.GetParameter<UInt2>(ParameterBox::MakeParameterNameHash("Dims"));
        if (dimsParam.first) viewDims = dimsParam.second;

        auto formatParam = parameters.GetString<char>(ParameterBox::MakeParameterNameHash("Format"));
        if (!formatParam.empty())
            rtFormat = Metal::AsNativeFormat(formatParam.c_str());

        rtFormat = AsRTFormat(rtFormat);

        if (!viewDims[0] || !viewDims[1] || rtFormat == Metal::NativeFormat::Unknown)
            Throw(::Exceptions::BasicLabel("Missing Dims or Format parameter"));

        auto dstDesc = TextureDesc::Plain2D(viewDims[0], viewDims[1], unsigned(rtFormat));

        auto i = fns.find(shaderName.AsString());
        if (i != fns.end()) {
            return (i->second)(dstDesc, parameters);
        } else {
            auto psShaderName = xleDir.AsString() + "/Working/game/xleres/" + shaderName.AsString();
            if (!XlFindStringI(psShaderName.c_str(), "ps_"))
                psShaderName += ":" PS_DefShaderModel;

            auto psByteCode = LoadShaderImmediate(psShaderName.c_str());
            auto vsByteCode = LoadShaderImmediate((xleDir.AsString() + "/Working/game/xleres/basic2D.vsh:fullscreen:" VS_DefShaderModel).c_str());

            Metal::ShaderProgram shaderProg(vsByteCode, psByteCode);
            Metal::BoundUniforms uniforms(shaderProg);
            uniforms.BindConstantBuffers(1, {"Material"});

            for (unsigned c = 0; c<inputResources.size(); ++c)
                uniforms.BindShaderResource(inputResources[c]._bindingHash, c, 1);

            Techniques::PredefinedCBLayout cbLayout(GetCBLayoutName(psShaderName.c_str()).c_str());

            auto& uploads = Samples::MinimalAssetServices::GetBufferUploads();
            auto dstTexture = uploads.Transaction_Immediate(
                CreateDesc(
                    BindFlag::RenderTarget,
                    0, GPUAccess::Write,
                    dstDesc, 
                    "TextureProcessOutput"));

            auto metalContext = Metal::DeviceContext::Get(*device.GetImmediateContext());
            metalContext->Bind(Metal::ViewportDesc(0.f, 0.f, float(viewDims[0]), float(viewDims[1])));
            Metal::RenderTargetView rtv(dstTexture->GetUnderlying());
            metalContext->Bind(MakeResourceList(rtv), nullptr);

            auto& commonRes = Techniques::CommonResources();
            metalContext->Bind(commonRes._cullDisable);
            metalContext->Bind(commonRes._blendOpaque);
            metalContext->Bind(commonRes._dssDisable);
            metalContext->BindPS(MakeResourceList(commonRes._defaultSampler));

            SharedPkt cbs[] = { cbLayout.BuildCBDataAsPkt(parameters) };
            std::vector<const Metal::ShaderResourceView*> srvs;
            for (const auto&i:inputResources) srvs.push_back(&i._srv);

            uniforms.Apply(
                *metalContext, Metal::UniformsStream(),
                Metal::UniformsStream(
                    cbs, nullptr, dimof(cbs),
                    AsPointer(srvs.begin()), srvs.size()));
            metalContext->Bind(shaderProg);

            metalContext->Bind(Metal::Topology::TriangleStrip);
            metalContext->Unbind<Metal::VertexBuffer>();
            metalContext->Unbind<Metal::BoundInputLayout>();
        
            metalContext->Draw(4);

            uniforms.UnbindShaderResources(*metalContext, 1);
            metalContext->Bind(ResourceList<Metal::RenderTargetView, 0>(), nullptr);

            auto result = uploads.Resource_ReadBack(*dstTexture);
            return TextureResult { result, unsigned(rtFormat), viewDims };
        }
    }

    void TextureResult::SaveTIFF(const ::Assets::ResChar destinationFile[]) const
    {
            // using DirectXTex to save to disk as a TGA file...
        auto rowPitch = _pkt->GetPitches()._rowPitch;
        DirectX::Image image {
            _dimensions[0], _dimensions[1],
            Metal::AsDXGIFormat(Metal::NativeFormat::Enum(_format)),
            rowPitch, rowPitch * _dimensions[1],
            (uint8_t*)_pkt->GetData() };
        auto fn = Conversion::Convert<std::wstring>(std::string(destinationFile));

        auto ext = MakeFileNameSplitter(destinationFile).Extension();

        HRESULT hresult;
        if (XlEqStringI(ext, "tga")) {
            hresult = DirectX::SaveToTGAFile(image, fn.c_str());
        } else if (XlEqStringI(ext, "dds")) {
            hresult = DirectX::SaveToDDSFile(image, DirectX::DDS_FLAGS_NONE, fn.c_str());
        } else {
            const GUID GUID_ContainerFormatTiff = 
                { 0x163bcc30, 0xe2e9, 0x4f0b, { 0x96, 0x1d, 0xa3, 0xe9, 0xfd, 0xb7, 0x88, 0xa3 }};
            const GUID GUID_ContainerFormatPng  = 
                { 0x1b7cfaf4, 0x713f, 0x473c, { 0xbb, 0xcd, 0x61, 0x37, 0x42, 0x5f, 0xae, 0xaf }};
            const GUID GUID_ContainerFormatBmp = 
                { 0x0af1d87e, 0xfcfe, 0x4188, { 0xbd, 0xeb, 0xa7, 0x90, 0x64, 0x71, 0xcb, 0xe3 }};

            GUID container = GUID_ContainerFormatTiff;

            if (XlEqStringI(ext, "bmp")) container = GUID_ContainerFormatBmp;
            else if (XlEqStringI(ext, "png")) container = GUID_ContainerFormatPng;

            hresult = DirectX::SaveToWICFile(
                image, DirectX::WIC_FLAGS_NONE,
                GUID_ContainerFormatTiff,
                fn.c_str());
        }

        if (!SUCCEEDED(hresult))
            Throw(::Exceptions::BasicLabel("Failure while written output image"));
    }

}