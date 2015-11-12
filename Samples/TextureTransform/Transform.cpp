// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

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

#include "../../RenderCore/DX11/Metal/DX11Utils.h"
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
                Metal::GetComponentPrecision(input));
        }
        return input;
    }

    static ::Assets::rstring GetCBLayout(const ::Assets::ResChar input[])
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
        BufferUploads::IManager& uploads,
        BufferUploads::ResourceLocator& source,
        StringSection<char> shader,
        const ParameterBox& shaderParameters)
    {
        using namespace BufferUploads;

        auto psShaderName = shader.AsString();
        if (!XlFindStringI(psShaderName.c_str(), "ps_"))
            psShaderName += ":" PS_DefShaderModel;

        auto psByteCode = LoadShaderImmediate(psShaderName.c_str());
        auto vsByteCode = LoadShaderImmediate("game/xleres/basic2D.vsh:fullscreen:" VS_DefShaderModel);

        const auto& cbLayout = ::Assets::GetAsset<Techniques::PredefinedCBLayout>(
            GetCBLayout(psShaderName.c_str()).c_str());
        Metal::TextureDesc2D textureDesc(source.GetUnderlying());
        auto rtFormat = AsRTFormat(Metal::AsNativeFormat(textureDesc.Format));

        UInt2 viewDims(textureDesc.Width, textureDesc.Height);

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
        metalContext->Bind(commonRes._defaultRasterizer);
        metalContext->Bind(commonRes._blendOpaque);
        metalContext->BindPS(MakeResourceList(commonRes._defaultSampler));
        
        Metal::ShaderProgram shaderProg(vsByteCode, psByteCode);
        Metal::BoundUniforms uniforms(shaderProg);
        uniforms.BindConstantBuffers(1, {"Material"});
        uniforms.BindShaderResources(1, {"Input"});

        Metal::ShaderResourceView inputSRV(source.GetUnderlying());
        uniforms.Apply(
            *metalContext, Metal::UniformsStream(),
            Metal::UniformsStream(
                { cbLayout.BuildCBDataAsPkt(shaderParameters) },
                { &inputSRV }));
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