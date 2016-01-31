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
#include "../../Utility/Threading/CompletionThreadPool.h"
#include "../../Utility/Threading/ThreadingUtils.h"
#include "../../Foreign/half-1.9.2/include/half.hpp"
#include <thread>
#include "../../Foreign/ISPCTex/ispc_texcomp.h"

#include "../../Foreign/DirectXTex/DirectXTex/DirectXTex.h" // (includes windows.h indirectly)
#undef max
#undef min

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

    static DirectX::ScratchImage PeformCompression_DXTex(
        const BufferUploads::TextureDesc& srcDesc, 
        BufferUploads::DataPacket& pkt,
        RenderCore::Metal::NativeFormat::Enum dstFormat)
    {
        // use SRGB compression for formats where it makes sense.
        const auto flags = DirectX::TEX_COMPRESS_SRGB | DirectX::TEX_COMPRESS_PARALLEL;

        auto metaData = AsMetadata(srcDesc);
        auto images = BuildImages(metaData, pkt);

        DirectX::ScratchImage scratchImage;
        auto hresult = DirectX::Compress(
            AsPointer(images.cbegin()), images.size(),
            metaData, 
            RenderCore::Metal::AsDXGIFormat(dstFormat),
            flags, 0.5f, scratchImage);
        if (!SUCCEEDED(hresult))
            Throw(::Exceptions::BasicLabel("Error while performing compression. Check formats"));

        return std::move(scratchImage);
    }

    inline uint32 FloatBits(float f) 
    {
        union { float f; unsigned i; } u;
        u.f = f;
        return u.i;
    }

#if 0
    static unsigned short AsUnsignedFloat16_Fast(float input)
    {
        // We can't use the "half" library for this -- because that only supports signed floats

            //
            //      See stack overflow article:
            //          http://stackoverflow.com/questions/3026441/float32-to-float16
            //
            //      He suggests either using a table lookup or vectorising
            //      this code for further optimisation.
            //
        unsigned int fltInt32 = FloatBits(input);
        
        unsigned short tmp = (fltInt32 >> 23) & 0xff;
        tmp = (tmp - 0x70) & ((unsigned int)((int)(0x70 - tmp) >> 4) >> 27);
        
        unsigned short fltInt16 = tmp << 11;
        fltInt16 |= (fltInt32 >> 12) & 0x7ff;
        
        // note -- no rounding performed!
        return fltInt16;    
    }
#endif

#if 1
    static unsigned short AsSignedFloat16_Fast(float input)
    {
        // unsigned int fltInt32 = FloatBits(input);
        // 
        // unsigned short fltInt16 = (fltInt32 >> 31) << 5;
        // 
        // unsigned short tmp = (fltInt32 >> 23) & 0xff;
        // tmp = (tmp - 0x70) & ((unsigned int)((int)(0x70 - tmp) >> 4) >> 27);
        // 
        // fltInt16 = (fltInt16 | tmp) << 10;
        // fltInt16 |= (fltInt32 >> 13) & 0x3ff;
        // 
        // return fltInt16;
        return half_float::detail::float2half<std::round_to_nearest>(input);
    }
#endif

    static DirectX::ScratchImage PeformCompression_IntelBC6H_UF16(
        const BufferUploads::TextureDesc& srcDesc,
        BufferUploads::DataPacket& pkt)
    {
        // We're going to the intel ispc compressor to create a BC6H_UF16 texture.
        // This code was originally distributed as a part of some sample code... So it's
        // not very flexible. Our version only takes 16 bit float RGBA inputs. Since the
        // output is also a 16bit float format, this makes sense. But we'll have to do
        // the conversion to 16 bit on this side.
        // We need to manually split up the input pkt into smaller blocks and do all of 
        // the thread management ourselves.
        bc6h_enc_settings settings;
        GetProfile_bc6h_veryslow(&settings);

        DirectX::ScratchImage result;
        auto scratchMetadata = AsMetadata(srcDesc);
        scratchMetadata.format = DXGI_FORMAT_BC6H_UF16;
        result.Initialize(scratchMetadata);

        // We're going to split the texture up into many smaller tasks.
        // We can't easily split the entire texture into a single task
        // per hardware thread, because the mipmaps make things difficult
        //  -- well, for a cubemap we could put one face on each processor,
        // but that would probably waste some processors. Let's just divide
        // each face into strips of some reasonable size (say, 16 blocks high).

        auto concurrency = std::thread::hardware_concurrency();
        CompletionThreadPool threadPool(concurrency);

        unsigned rowsPerStrip = 64; // normally input textures will be large, so we should have wide strips
        unsigned totalTaskCount = 0;
        Interlocked::Value completedTaskCount = 0;
        auto completedEvent = XlCreateEvent(true);

        for (unsigned a=0; a<srcDesc._arrayCount; ++a)
            for (unsigned m=0; m<srcDesc._mipCount; ++m) {
                unsigned mipHeight = std::max(4u, srcDesc._height >> m);
                totalTaskCount += std::max(1u, mipHeight/rowsPerStrip);
            }

        const auto srcFormat = srcDesc._nativePixelFormat;

        for (unsigned a=0; a<srcDesc._arrayCount; ++a)
            for (unsigned m=0; m<srcDesc._mipCount; ++m) {
                auto subRes = BufferUploads::DataPacket::TexSubRes(m, a);
                auto* srcData = pkt.GetData(subRes);
                auto srcPitches = pkt.GetPitches(subRes);
                auto* dstImage = result.GetImage(m, a, 0);
                unsigned mipHeight = std::max(4u, srcDesc._height >> m);

                for (unsigned s=0; s<std::max(1u, mipHeight/rowsPerStrip); ++s) {
                    rgba_surface src 
                    {
                        (uint8_t*)PtrAdd(srcData, s*rowsPerStrip*srcPitches._rowPitch), 
                        std::max(4u, srcDesc._width >> m),
                        std::min((s+1)*rowsPerStrip, mipHeight) - s*rowsPerStrip,
                        srcPitches._rowPitch
                    };
                    assert((s*rowsPerStrip+src.height) <= mipHeight);
                    assert(src.height%4==0);
                    auto* dst = PtrAdd(dstImage->pixels, (s*rowsPerStrip/4)*dstImage->rowPitch);

                    threadPool.Enqueue(
                        [totalTaskCount, &completedTaskCount, completedEvent, src, dst, &settings, srcFormat]()
                        {
                            // todo -- we may need to copy smaller mipmaps into an extra buffer
                            //      (to make sure there is always at least 1 full 4x4 block)
                            auto modSrc = src;

                            std::unique_ptr<uint8[]> midwayBuffer;
                            if (srcFormat == RenderCore::Metal::NativeFormat::R32G32B32A32_FLOAT
                                || srcFormat == RenderCore::Metal::NativeFormat::R32G32B32_FLOAT) {
                    
                                const unsigned midwayPixelPitch = 8;
                                const unsigned srcPixelPitch = (srcFormat == RenderCore::Metal::NativeFormat::R32G32B32A32_FLOAT?4:3)*4;                    
                                midwayBuffer = std::make_unique<uint8[]>(src.width*src.height*midwayPixelPitch);
                    
                                // convert 32 bit float -> unsigned 16 bit float
                                // Note there is no sign bit. We want 5 exponent bits + 11 mantissa bits
                                for (unsigned y=0; y<unsigned(src.height); ++y)
                                    for (unsigned x = 0; x<unsigned(src.width); ++x) {
                                        auto* s = (float*)PtrAdd(src.ptr, (y*src.stride) + x*srcPixelPitch);
                                        auto* d = (uint16*)PtrAdd(midwayBuffer.get(), (y*unsigned(src.width)*midwayPixelPitch) + x*midwayPixelPitch);
                                        d[0] = AsSignedFloat16_Fast(s[0]);
                                        d[1] = AsSignedFloat16_Fast(s[1]);
                                        d[2] = AsSignedFloat16_Fast(s[2]);
                                        d[3] = 0;   // alpha not supported
                                    }

                                modSrc.ptr = midwayBuffer.get();
                                modSrc.stride = src.width*midwayPixelPitch;
                            }
                
                            CompressBlocksBC6H(&modSrc, dst, &settings);

                            auto newCompletedCount = 1+Interlocked::Increment(&completedTaskCount);
                            if (unsigned(newCompletedCount) == totalTaskCount)
                                XlSetEvent(completedEvent);
                        });
                }
            }

        // wait for all tasks to complete (last one should trigger the event)
        XlWaitForSyncObject(completedEvent, XL_INFINITE);
        XlCloseSyncObject(completedEvent);

        return std::move(result);
    }

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

        // interpret input as a SRGB format (if possible)
        // todo -- handle SRGB input flags
        srcDesc._nativePixelFormat = (unsigned)RenderCore::Metal::AsSRGBFormat(
            RenderCore::Metal::NativeFormat::Enum(srcDesc._nativePixelFormat));

        auto dstFormat = RenderCore::Metal::NativeFormat::Enum(desc._nativePixelFormat);
        auto srcFormat = RenderCore::Metal::NativeFormat::Enum(srcDesc._nativePixelFormat);

        DirectX::ScratchImage scratchImage;
        if (dstFormat == RenderCore::Metal::NativeFormat::BC6H_UF16
            && (srcFormat == RenderCore::Metal::NativeFormat::R16G16B16A16_FLOAT
                || srcFormat == RenderCore::Metal::NativeFormat::R32G32B32A32_FLOAT
                || srcFormat == RenderCore::Metal::NativeFormat::R32G32B32_FLOAT)) {
            // When writing to BC6H_UF16, prefer to use the intel compressor. This compressor
            // is both much faster and more accurate. However, it doesn't support a wide range of
            // input formats -- our version will only work with 
            scratchImage = PeformCompression_IntelBC6H_UF16(srcDesc, *pkt);
        } else
            scratchImage = PeformCompression_DXTex(srcDesc, *pkt, dstFormat);

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

