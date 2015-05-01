// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DataPacket.h"
#include "PlatformInterface.h"

namespace BufferUploads
{
    auto DataPacket::TexSubRes(unsigned mipIndex, unsigned arrayIndex) -> SubResource
    {
        return (mipIndex & 0xffff) | (arrayIndex << 16);
    }

    BasicRawDataPacket::BasicRawDataPacket(size_t dataSize, const void* data, TexturePitches pitches)
    : _dataSize(dataSize), _pitches(pitches)    
    {
        _data.reset((uint8*)XlMemAlign(dataSize, 16));

            // note --  prefer sending aligned data as input! Just makes it
            //          more convenient for copying
        if (data) {
            if ((size_t(data) & 0xf)==0x0 && (_dataSize & 0xf)==0x0) {
                XlCopyMemoryAlign16(_data.get(), data, _dataSize);
            } else {
                XlCopyMemory(_data.get(), data, _dataSize);
            }
        }
    }

    BasicRawDataPacket::~BasicRawDataPacket()
    {}

    void* BasicRawDataPacket::GetData(SubResource subRes)
    {
        assert(subRes == 0);
        return _data.get(); 
    }
    
    size_t BasicRawDataPacket::GetDataSize(SubResource subRes) const
    {
        assert(subRes == 0);
        return _dataSize; 
    }

    TexturePitches BasicRawDataPacket::GetPitches(SubResource subRes) const
    {
        assert(subRes == 0);
        return _pitches; 
    }

    auto BasicRawDataPacket::BeginBackgroundLoad() const -> std::shared_ptr<Marker> { return nullptr; }

    intrusive_ptr<DataPacket> CreateBasicPacket(
        size_t dataSize, const void* data, TexturePitches rowAndSlicePitch)
    {
        return make_intrusive<BasicRawDataPacket>(dataSize, data, rowAndSlicePitch);
    }

    static const unsigned BlockCompDim = 4;
    static unsigned RoundBCDim(unsigned input)
    {
        uint32 part = input%BlockCompDim;
        auto result = input + part?(BlockCompDim-part):0;
        assert(!(result%BlockCompDim));
        return result;
    }

    TexturePitches::TexturePitches(const TextureDesc& desc)
    {
        using namespace RenderCore::Metal;
        _slicePitch = PlatformInterface::TextureDataSize(
            desc._width, desc._height, 1, 1, 
            (NativeFormat::Enum)desc._nativePixelFormat);
            
            //  row pitch calculation is a little platform-specific here...
            //  (eg, DX9 and DX11 use different systems)
            //  Perhaps this could be moved into the platform interface layer
        bool isDXT = GetCompressionType(NativeFormat::Enum(desc._nativePixelFormat)) 
            == FormatCompressionType::BlockCompression;
        if (isDXT) {
            _rowPitch = PlatformInterface::TextureDataSize(
                RoundBCDim(desc._width), BlockCompDim, 1, 1, 
                (NativeFormat::Enum)desc._nativePixelFormat);
        } else {
            _rowPitch = PlatformInterface::TextureDataSize(
                desc._width, 1, 1, 1, 
                (NativeFormat::Enum)desc._nativePixelFormat);
        }
    }

    intrusive_ptr<DataPacket> CreateEmptyPacket(const BufferDesc& desc)
    {
            // Create an empty packet of the appropriate size for the given desc
            // Linear buffers are simple, but textures need a little more detail...
        if (desc._type == BufferDesc::Type::LinearBuffer) {
            auto size = PlatformInterface::ByteCount(desc);
            return make_intrusive<BasicRawDataPacket>(size, nullptr, TexturePitches(size, size));
        } else if (desc._type == BufferDesc::Type::Texture) {
                //  currently not supporting textures with multiple mip-maps
                //  or multiple array slices
            assert(desc._textureDesc._mipCount <= 1);
            assert(desc._textureDesc._arrayCount <= 1);

            TexturePitches pitches(desc._textureDesc);
            return make_intrusive<BasicRawDataPacket>(pitches._slicePitch, nullptr, pitches);
        }

        return nullptr;
    }

}

