// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DataPacket.h"
#include "PlatformInterface.h"

namespace BufferUploads
{
    BasicRawDataPacket::BasicRawDataPacket(size_t dataSize, const void* data, std::pair<unsigned,unsigned> rowAndSlicePitch)
    : _dataSize(dataSize), _rowAndSlicePitch(rowAndSlicePitch)    
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

    void* BasicRawDataPacket::GetData(unsigned mipIndex, unsigned arrayIndex)
    {
        assert(mipIndex == 0 && arrayIndex == 0);
        return _data.get(); 
    }
    
    size_t BasicRawDataPacket::GetDataSize(unsigned mipIndex, unsigned arrayIndex) const
    {
        assert(mipIndex == 0 && arrayIndex == 0);
        return _dataSize; 
    }

    std::pair<unsigned,unsigned> BasicRawDataPacket::GetRowAndSlicePitch(unsigned mipIndex, unsigned arrayIndex) const
    {
        assert(mipIndex == 0 && arrayIndex == 0);
        return _rowAndSlicePitch; 
    }

    intrusive_ptr<RawDataPacket> CreateBasicPacket(
        size_t dataSize, const void* data, std::pair<unsigned,unsigned> rowAndSlicePitch)
    {
        return make_intrusive<BasicRawDataPacket>(dataSize, data, rowAndSlicePitch);
    }

    static const unsigned BlockCompDim = 4;
    static unsigned    RoundBCDim(unsigned input)
    {
        uint32 part = input%BlockCompDim;
        auto result = input + part?(BlockCompDim-part):0;
        assert(!(result%BlockCompDim));
        return result;
    }

    intrusive_ptr<RawDataPacket> CreateEmptyPacket(const BufferDesc& desc)
    {
            // Create an empty packet of the appropriate size for the given desc
            // Linear buffers are simple, but textures need a little more detail...
        if (desc._type == BufferDesc::Type::LinearBuffer) {
            auto size = PlatformInterface::ByteCount(desc);
            return make_intrusive<BasicRawDataPacket>(size, nullptr, std::make_pair(size, size));
        } else if (desc._type == BufferDesc::Type::Texture) {
                //  currently not supporting textures with multiple mip-maps
                //  or multiple array slices
            assert(desc._textureDesc._mipCount <= 1);
            assert(desc._textureDesc._arrayCount <= 1);

            using namespace RenderCore::Metal;
            unsigned slicePitch = PlatformInterface::TextureDataSize(
                desc._textureDesc._width, desc._textureDesc._height, 1, 1, 
                (NativeFormat::Enum)desc._textureDesc._nativePixelFormat);
            unsigned rowPitch;
            
                //  row pitch calculation is a little platform-specific here...
                //  (eg, DX9 and DX11 use different systems)
                //  Perhaps this could be moved into the platform interface layer
            bool isDXT = GetCompressionType(NativeFormat::Enum(desc._textureDesc._nativePixelFormat)) 
                == FormatCompressionType::BlockCompression;
            if (isDXT) {
                rowPitch = PlatformInterface::TextureDataSize(
                    RoundBCDim(desc._textureDesc._width), BlockCompDim, 1, 1, 
                    (NativeFormat::Enum)desc._textureDesc._nativePixelFormat);
            } else {
                rowPitch = PlatformInterface::TextureDataSize(
                    desc._textureDesc._width, 1, 1, 1, 
                    (NativeFormat::Enum)desc._textureDesc._nativePixelFormat);
            }

            return make_intrusive<BasicRawDataPacket>(slicePitch, nullptr, std::make_pair(rowPitch, slicePitch));
        }

        return nullptr;
    }

}

