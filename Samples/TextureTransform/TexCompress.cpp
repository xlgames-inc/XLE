// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Transform.h"
#include "../../RenderCore/Metal/Format.h"
#include "../../Assets/AssetsCore.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/Conversion.h"
#include "../../Utility/StringUtils.h"
#include "../../Foreign/DirectXTex/DirectXTex/DirectXTex.h"

namespace TextureTransform
{
    DirectX::TexMetadata AsMetadata(
        const BufferUploads::TextureDesc& desc);

    std::vector<DirectX::Image> BuildImages(
        const DirectX::TexMetadata& metaData, 
        BufferUploads::DataPacket& pkt);

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ScratchImagePacket : public BufferUploads::DataPacket
    {
    public:
        virtual void*   GetData         (SubResource subRes);
        virtual size_t  GetDataSize     (SubResource subRes) const;
        virtual auto    GetPitches      (SubResource subRes) const -> BufferUploads::TexturePitches;
        virtual std::shared_ptr<Marker>     BeginBackgroundLoad();

        ScratchImagePacket(DirectX::ScratchImage&& moveFrom);
        ~ScratchImagePacket();
    private:
        DirectX::ScratchImage _image;
    };

    void*   ScratchImagePacket::GetData         (SubResource subRes)
    {
        auto arrayIndex = subRes >> 16u, mip = subRes & 0xffffu;
        auto* image = _image.GetImage(mip, arrayIndex, 0);
        assert(image);
        return image->pixels;
    }

    size_t  ScratchImagePacket::GetDataSize     (SubResource subRes) const
    {
        auto arrayIndex = subRes >> 16u, mip = subRes & 0xffffu;
        auto* image = _image.GetImage(mip, arrayIndex, 0);
        assert(image);
        return image->slicePitch;
    }

    auto    ScratchImagePacket::GetPitches      (SubResource subRes) const 
        -> BufferUploads::TexturePitches
    {
        auto arrayIndex = subRes >> 16u, mip = subRes & 0xffffu;
        auto* image = _image.GetImage(mip, arrayIndex, 0);
        assert(image);
        return BufferUploads::TexturePitches(unsigned(image->rowPitch), unsigned(image->slicePitch));
    }

    auto    ScratchImagePacket::BeginBackgroundLoad() -> std::shared_ptr<Marker> { return nullptr; }

    ScratchImagePacket::ScratchImagePacket(DirectX::ScratchImage&& moveFrom)
    : _image(std::move(moveFrom))
    {}

    ScratchImagePacket::~ScratchImagePacket() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    TextureResult CompressTexture(
        const BufferUploads::TextureDesc& desc, 
        const ParameterBox& parameters)
    {
        auto inputTexture = parameters.GetString<char>(ParameterBox::MakeParameterNameHash("Input"));
        if (inputTexture.empty())
            Throw(::Exceptions::BasicLabel("Expecting 'Input' parameter"));

        auto inputAssetName = Conversion::Convert<::Assets::rstring>(inputTexture);
        auto srcDesc = BufferUploads::LoadTextureFormat(MakeStringSection(inputAssetName));
        if (srcDesc._width == 0)
            Throw(::Exceptions::BasicLabel("Failure while loading input texture"));
            
        auto pkt = BufferUploads::CreateStreamingTextureSource(MakeStringSection(inputAssetName), 0);
        auto load = pkt->BeginBackgroundLoad();
        auto state = load->StallWhilePending();
        if (state != ::Assets::AssetState::Ready)
            Throw(::Exceptions::BasicLabel("Failure while loading input texture"));

        // use SRGB compression for formats where it makes sense.
        const auto flags = DirectX::TEX_COMPRESS_SRGB | DirectX::TEX_COMPRESS_PARALLEL;

        // interpret input as a SRGB format (if possible)
        // todo -- handle SRGB input flags
        srcDesc._nativePixelFormat = (unsigned)RenderCore::Metal::AsSRGBFormat(
            RenderCore::Metal::NativeFormat::Enum(srcDesc._nativePixelFormat));

        auto metaData = AsMetadata(srcDesc);
        auto images = BuildImages(metaData, *pkt);

        DirectX::ScratchImage scratchImage;
        auto hresult = DirectX::Compress(
            AsPointer(images.cbegin()), images.size(),
            metaData, 
            RenderCore::Metal::AsDXGIFormat(
                RenderCore::Metal::NativeFormat::Enum(desc._nativePixelFormat)),
            flags, 0.5f, scratchImage);
        if (!SUCCEEDED(hresult))
            Throw(::Exceptions::BasicLabel("Error while performing compression. Check formats"));

        // We need to convert the output to a TextureResult to pass back. 
        // This requires creating a special case DataPacket that wraps the ScratchImage object
        auto format = scratchImage.GetMetadata().format;
        auto dims = UInt2(
            unsigned(scratchImage.GetMetadata().width), 
            unsigned(scratchImage.GetMetadata().height));
        auto mipCount = (unsigned)scratchImage.GetMetadata().mipLevels;
        auto arrayCount = (unsigned)scratchImage.GetMetadata().arraySize;
        auto resultPkt = make_intrusive<ScratchImagePacket>(std::move(scratchImage));
        return TextureResult
            {
                std::move(resultPkt),
                unsigned(format), dims, mipCount, arrayCount
            };
    }
}

