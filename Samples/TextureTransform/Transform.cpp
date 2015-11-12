// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

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

#include "../../Core/WinAPI/IncludeWindows.h"
#include "../../Foreign/DirectXTex/DirectXTex/DirectXTex.h"

namespace TextureTransform
{
    using namespace RenderCore;

    static CompiledShaderByteCode LoadShaderImmediate(const ::Assets::ResChar name[])
    {
        CompiledShaderByteCode compiledByteCode(name);
        auto state = compiledByteCode.StallWhilePending();
        if (state == ::Assets::AssetState::Invalid)
            Throw(::Assets::Exceptions::InvalidAsset(name, "Failure while loading or compiling shader byte code"));
        return std::move(compiledByteCode);
    }

    class InputResource
    {
    public:
        Metal::ShaderResourceView _srv;
        intrusive_ptr<BufferUploads::ResourceLocator> _resLocator;
        BufferUploads::TextureDesc _desc;
        Metal::NativeFormat::Enum _finalFormat;

        InputResource(const ::Assets::ResChar initializer[]);
        ~InputResource();
    private:
        enum class SourceColorSpace { SRGB, Linear, Unspecified };
    };

    InputResource::InputResource(const ::Assets::ResChar initializer[])
    {
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

        _desc = ExtractDesc(*_resLocator->GetUnderlying())._textureDesc;

        auto format = (Metal::NativeFormat::Enum)_desc._nativePixelFormat;
        if (colSpace == SourceColorSpace::SRGB) format = Metal::AsSRGBFormat(format);
        else if (colSpace == SourceColorSpace::Linear) format = Metal::AsLinearFormat(format);
        _srv = Metal::ShaderResourceView(_resLocator->GetUnderlying(), format);
        _finalFormat = format;
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

    class TextureResult
    {
    public:
        intrusive_ptr<BufferUploads::DataPacket> _pkt;
        unsigned _format;
        UInt2 _dimensions;

        void SaveTIFF(const ::Assets::ResChar destinationFile[]) const;
    };

    TextureResult ExecuteTransform(
        IDevice& device,
        StringSection<char> xleDir,
        StringSection<char> shaderName,
        const ParameterBox& parameters)
    {
        using namespace BufferUploads;

        auto psShaderName = xleDir.AsString() + "/Working/game/xleres/" + shaderName.AsString();
        if (!XlFindStringI(psShaderName.c_str(), "ps_"))
            psShaderName += ":" PS_DefShaderModel;

        auto psByteCode = LoadShaderImmediate(psShaderName.c_str());
        auto vsByteCode = LoadShaderImmediate((xleDir.AsString() + "/Working/game/xleres/basic2D.vsh:fullscreen:" VS_DefShaderModel).c_str());

        Metal::ShaderProgram shaderProg(vsByteCode, psByteCode);
        Metal::BoundUniforms uniforms(shaderProg);
        uniforms.BindConstantBuffers(1, {"Material"});

        std::vector<InputResource> inputResources;

            // We'll interpret every string in the input parameters as a resource name
        for (auto i=parameters.Begin(); !i.IsEnd(); ++i) {
            if (i.Type()._typeHint == ImpliedTyping::TypeHint::String) {
                auto value = ImpliedTyping::AsString(
                    i.RawValue(), ptrdiff_t(i.ValueTableEnd()) - ptrdiff_t(i.RawValue()), i.Type());

                InputResource inputRes(value.c_str());
                if (inputRes._srv.IsGood()) {
                    uniforms.BindShaderResource(Hash64((const char*)i.Name()), (unsigned)inputResources.size(), 1);
                    inputResources.push_back(std::move(inputRes));
                }
            }
        }

            // we need to calculate an output format and viewport dimensions
            // if we have at least one input resource, we will duplicate that resource;
        if (inputResources.empty())
            Throw(::Exceptions::BasicLabel("No input resources specified on command line (or input resources not found)"));

        auto rtFormat = AsRTFormat(inputResources[0]._finalFormat);
        UInt2 viewDims(inputResources[0]._desc._width, inputResources[0]._desc._height);

        Techniques::PredefinedCBLayout cbLayout(GetCBLayoutName(psShaderName.c_str()).c_str());

        auto& uploads = Samples::MinimalAssetServices::GetBufferUploads();
        auto dstTexture = uploads.Transaction_Immediate(
            CreateDesc(
                BindFlag::RenderTarget,
                0, GPUAccess::Write,
                TextureDesc::Plain2D(viewDims[0], viewDims[1], unsigned(rtFormat)), 
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

        return TextureResult {
            uploads.Resource_ReadBack(*dstTexture),
            unsigned(rtFormat), viewDims };
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

        const GUID GUID_ContainerFormatTiff = 
            { 0x163bcc30, 0xe2e9, 0x4f0b, { 0x96, 0x1d, 0xa3, 0xe9, 0xfd, 0xb7, 0x88, 0xa3 }};
        auto hresult = DirectX::SaveToWICFile(
            image, DirectX::WIC_FLAGS_NONE,
            GUID_ContainerFormatTiff,
            fn.c_str());
        (void)hresult;
        assert(SUCCEEDED(hresult));
    }

}