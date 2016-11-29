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
#include "../../RenderCore/Format.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/TextureView.h"
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
		Format _finalFormat;
        uint64 _bindingHash;

        InputResource(const ::Assets::ResChar initializer[]);
        ~InputResource();
    private:
        enum class SourceColorSpace { SRGB, Linear, Unspecified };
    };

    InputResource::InputResource(const ::Assets::ResChar initializer[])
    {
        _finalFormat = Format::Unknown;
        _desc = BufferUploads::TextureDesc::Plain1D(0, Format::Unknown);
        _bindingHash = 0;

        auto splitter = MakeFileNameSplitter(initializer);

        SourceColorSpace colSpace = SourceColorSpace::Unspecified;
        for (auto c:splitter.Parameters()) {
            if (c == 'l' || c == 'L') { colSpace = SourceColorSpace::Linear; }
            if (c == 's' || c == 'S') { colSpace = SourceColorSpace::SRGB; }
        }

        if (colSpace == SourceColorSpace::Unspecified)
            colSpace = XlFindStringI(initializer, "_ddn") ? SourceColorSpace::Linear : SourceColorSpace::SRGB;
        
        using namespace RenderCore;
        auto& uploads = Samples::MinimalAssetServices::GetBufferUploads();
        auto inputPacket = BufferUploads::CreateStreamingTextureSource(splitter.AllExceptParameters());
        _resLocator = uploads.Transaction_Immediate(
            CreateDesc(
                BindFlag::ShaderResource,
                0, GPUAccess::Read,
                TextureDesc::Empty(), "TextureProcessInput"),
            inputPacket.get());

        if (_resLocator) {
            _desc = Metal::ExtractDesc(_resLocator->GetUnderlying())._textureDesc;

            auto format = (Format)_desc._format;
            if (colSpace == SourceColorSpace::SRGB) format = AsSRGBFormat(format);
            else if (colSpace == SourceColorSpace::Linear) format = AsLinearFormat(format);
			_srv = Metal::ShaderResourceView(_resLocator->GetUnderlying(), TextureViewWindow{format});
            _finalFormat = format;
        }
    }

    InputResource::~InputResource(){}

    static Format AsRTFormat(Format input)
    {
        // Convert the input format into a format that we can write to.
        // Normally this should mean just converting a compressed format into
        // a plain RGBA format.
        auto compression = GetCompressionType(input);
        if (compression == FormatCompressionType::BlockCompression) {
            return FindFormat(
                FormatCompressionType::None,
                GetComponents(input),
                GetComponentType(input),
                GetDecompressedComponentPrecision(input));
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

        auto rtFormat = Format::Unknown;
        UInt2 viewDims(0, 0);
        unsigned arrayCount = 1;
        unsigned mipCount = 1;
        unsigned passCount = 1;
        if (!inputResources.empty()) {
            rtFormat = inputResources[0]._finalFormat;
            viewDims = UInt2(inputResources[0]._desc._width, inputResources[0]._desc._height);
        }

        auto dimsParam = parameters.GetParameter<UInt2>(ParameterBox::MakeParameterNameHash("Dims"));
        if (dimsParam.first) viewDims = dimsParam.second;

        auto formatParam = parameters.GetString<char>(ParameterBox::MakeParameterNameHash("Format"));
        if (!formatParam.empty())
            rtFormat = AsFormat(formatParam.c_str());

        arrayCount = std::max(1u, parameters.GetParameter(ParameterBox::MakeParameterNameHash("ArrayCount"), arrayCount));
        mipCount = std::max(1u, parameters.GetParameter(ParameterBox::MakeParameterNameHash("MipCount"), mipCount));
        passCount = std::max(1u, parameters.GetParameter(ParameterBox::MakeParameterNameHash("PassCount"), passCount));
        
        if (!viewDims[0] || !viewDims[1] || rtFormat == Format::Unknown)
            Throw(::Exceptions::BasicLabel("Missing Dims or Format parameter"));

        auto dstDesc = RenderCore::TextureDesc::Plain2D(
            viewDims[0], viewDims[1], rtFormat, 
            uint8(mipCount), uint16(arrayCount));

        auto i = fns.find(shaderName.AsString());
        if (i != fns.end()) {
            return (i->second)(dstDesc, parameters);
        } else {
            rtFormat = AsRTFormat(rtFormat);
            dstDesc._format = rtFormat;
            if (rtFormat == Format::Unknown)
                Throw(::Exceptions::BasicLabel("Could not find match pixel format for render target. Check inputs."));

            auto psShaderName = xleDir.AsString() + "/Working/game/xleres/" + shaderName.AsString();
            if (!XlFindStringI(psShaderName.c_str(), "ps_"))
                psShaderName += ":" PS_DefShaderModel;

            auto psByteCode = LoadShaderImmediate(psShaderName.c_str());
            auto vsByteCode = LoadShaderImmediate((xleDir.AsString() + "/Working/game/xleres/basic2D.vsh:fullscreen:" VS_DefShaderModel).c_str());

            Metal::ShaderProgram shaderProg(vsByteCode, psByteCode);
            Metal::BoundUniforms uniforms(shaderProg);
            uniforms.BindConstantBuffers(1, {"Material", "SubResourceId"});

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
            auto& commonRes = Techniques::CommonResources();
            metalContext->Bind(commonRes._cullDisable);
            metalContext->Bind(commonRes._dssDisable);
            metalContext->BindPS(MakeResourceList(commonRes._defaultSampler));
            metalContext->Bind(shaderProg);
            metalContext->Bind(Topology::TriangleStrip);
            metalContext->Unbind<Metal::VertexBuffer>();
            metalContext->Unbind<Metal::BoundInputLayout>();

            for (unsigned m=0; m<mipCount; ++m)
                for (unsigned a=0; a<arrayCount; ++a) {
                    UInt2 mipDims(std::max(1u, viewDims[0] >> m), std::max(1u, viewDims[1] >> m));
                    metalContext->Bind(Metal::ViewportDesc(0.f, 0.f, float(mipDims[0]), float(mipDims[1])));
                    Metal::RenderTargetView rtv(
                        dstTexture->GetUnderlying(),
                        TextureViewWindow(
							rtFormat, RenderCore::TextureDesc::Dimensionality::Undefined,
							RenderCore::TextureViewWindow::SubResourceRange{m, 1},
							RenderCore::TextureViewWindow::SubResourceRange{a, 1}));
                    metalContext->Bind(MakeResourceList(rtv), nullptr);

                    for (unsigned p=0; p<passCount; ++p) {

                        metalContext->Bind((p==0)?commonRes._blendOpaque:commonRes._blendAdditive);

                        struct SubResourceId
                        {
                            unsigned _arrayIndex, _mipIndex;
                            unsigned _passIndex, _passCount;
                        } subResId = { a, m, p, passCount };

                        SharedPkt cbs[] = { cbLayout.BuildCBDataAsPkt(parameters), MakeSharedPkt(subResId) };
                        std::vector<const Metal::ShaderResourceView*> srvs;
                        for (const auto&i:inputResources) srvs.push_back(&i._srv);

                        uniforms.Apply(
                            *metalContext, Metal::UniformsStream(),
                            Metal::UniformsStream(
                                cbs, nullptr, dimof(cbs),
                                AsPointer(srvs.begin()), srvs.size()));
        
                        metalContext->Draw(4);

                        // We're sometimes getting driver resets while performing long operations. This can happen
                        // when a draw operation takes a long time, and windows thinks the GPU has crashed up.
                        // Let's try to split the draw operations up a bit, so windows is less paranoid!
                        if ((p%8)==7) {
                            metalContext->GetUnderlying()->Flush();
                            Threading::Sleep(16);
                        }
                    }
                }

            uniforms.UnbindShaderResources(*metalContext, 1);
            metalContext->Bind(ResourceList<Metal::RenderTargetView, 0>(), nullptr);

            auto result = uploads.Resource_ReadBack(*dstTexture);
            return TextureResult { result, rtFormat, viewDims, mipCount, arrayCount };
        }
    }

    DirectX::TexMetadata AsMetadata(const BufferUploads::TextureDesc& desc)
    {
        DirectX::TexMetadata metaData;
        metaData.width = desc._width;
        metaData.height = desc._height;
        metaData.depth = desc._depth;
        metaData.arraySize = desc._arrayCount;
        metaData.mipLevels = desc._mipCount;
        metaData.miscFlags = 0;
        metaData.miscFlags2 = 0;
        metaData.format = Metal::AsDXGIFormat(desc._format);
        metaData.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;

            // Let's assume that all 6 element arrays are cubemaps. 
            // We can set "TEX_MISC_TEXTURECUBE" to mark it as a cubemap
        if (metaData.arraySize == 6)
            metaData.miscFlags |= DirectX::TEX_MISC_TEXTURECUBE;
        return metaData;
    }

    std::vector<DirectX::Image> BuildImages(
        const DirectX::TexMetadata& metaData,
        BufferUploads::DataPacket& pkt)
    {
        std::vector<DirectX::Image> images;
        images.reserve(metaData.mipLevels * metaData.arraySize);
        for (unsigned a=0; a<metaData.arraySize; ++a) {
            for (unsigned m=0; m<metaData.mipLevels; ++m) {
				auto subRes = SubResourceId {m, a};
                auto rowPitch = pkt.GetPitches(subRes)._rowPitch;
                auto slicePitch = pkt.GetPitches(subRes)._slicePitch;
                UInt2 mipDims(
                    std::max(1u, unsigned(metaData.width >> m)), 
                    std::max(1u, unsigned(metaData.height >> m)));
                images.push_back({
                    mipDims[0], mipDims[1],
                    metaData.format,
                    rowPitch, slicePitch,
                    (uint8_t*)pkt.GetData(subRes) });
            }
        }

        return std::move(images);
    }

    void TextureResult::Save(const ::Assets::ResChar destinationFile[]) const
    {
            // using DirectXTex to save to disk...
            // This provides a nice layer over a number of underlying formats
        auto ext = MakeFileNameSplitter(destinationFile).Extension();
        auto fn = Conversion::Convert<std::basic_string<utf16>>(std::string(destinationFile));

        bool singleSubresource = (_mipCount <= 1) && (_arrayCount <= 1);
        if (singleSubresource) {
            auto rowPitch = _pkt->GetPitches()._rowPitch;
            auto slicePitch = _pkt->GetPitches()._slicePitch;
            DirectX::Image image {
                _dimensions[0], _dimensions[1],
                Metal::AsDXGIFormat(_format),
                rowPitch, slicePitch,
                (uint8_t*)_pkt->GetData() };
            
            HRESULT hresult;
            if (XlEqStringI(ext, "tga")) {
                hresult = DirectX::SaveToTGAFile(image, (const wchar_t*)fn.c_str());
            } else if (XlEqStringI(ext, "dds")) {
				DirectX::TexMetadata mdata;
				memset( &mdata, 0, sizeof(mdata) );
				mdata.width = image.width;
				mdata.height = image.height;
				mdata.depth = 1;
				mdata.arraySize = 1;
				mdata.mipLevels = 1;
				mdata.format = image.format;
				mdata.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;
				
				// Assume that a single image with height=1 is a 1D texture;
				unsigned flags = DirectX::DDS_FLAGS_NONE;
				if (image.height == 1) {
					mdata.dimension = DirectX::TEX_DIMENSION_TEXTURE1D;
					flags |= DirectX::DDS_FLAGS_FORCE_DX10_EXT;	// DX10 header required to mark it as a 1D texture
				}

                hresult = DirectX::SaveToDDSFile(&image, 1, mdata, flags, (const wchar_t*)fn.c_str());
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
					(const wchar_t*)fn.c_str());
            }

            if (!SUCCEEDED(hresult))
                Throw(::Exceptions::BasicLabel("Failure while written output image"));
        } else {

            // When we have multiple mipmaps or array elements, we're creating a multi subresource
            // texture. These can only be saved in DDS format.
            if (!XlEqStringI(ext, "dds"))
                Throw(::Exceptions::BasicLabel("Multi-subresource textures must be saved in DDS format"));
            
            auto metaData = AsMetadata(
                BufferUploads::TextureDesc::Plain2D(
                    _dimensions[0], _dimensions[1], _format, 
                    uint8(_mipCount), uint16(_arrayCount)));
            auto images = BuildImages(metaData, *_pkt);

            auto hresult = DirectX::SaveToDDSFile(
                AsPointer(images.cbegin()), images.size(), 
                metaData,
                DirectX::DDS_FLAGS_NONE, (const wchar_t*)fn.c_str());

            if (!SUCCEEDED(hresult))
                Throw(::Exceptions::BasicLabel("Failure while written output image"));
        }
    }

}