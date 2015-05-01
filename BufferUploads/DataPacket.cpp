// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DataPacket.h"
#include "PlatformInterface.h"

namespace BufferUploads
{

///////////////////////////////////////////////////////////////////////////////////////////////////

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

    auto BasicRawDataPacket::BeginBackgroundLoad() -> std::shared_ptr<Marker> { return nullptr; }

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

///////////////////////////////////////////////////////////////////////////////////////////////////
            //      S T R E A M I N G   P A C K E T

    class FileDataSource : public DataPacket
    {
    public:
        virtual void*           GetData         (SubResource subRes);
        virtual size_t          GetDataSize     (SubResource subRes) const;
        virtual TexturePitches  GetPitches      (SubResource subRes) const;

        virtual std::shared_ptr<Marker>     BeginBackgroundLoad();

        FileDataSource(const void* fileHandle, size_t offset, size_t dataSize, TexturePitches pitches);
        virtual ~FileDataSource();

    protected:
        HANDLE      _fileHandle;
        size_t      _dataSize;
        size_t      _offset;

        struct SpecialOverlapped
        {
            OVERLAPPED                      _internal;
            intrusive_ptr<FileDataSource>   _returnPointer;
        };
        SpecialOverlapped _overlappedStatus;
        std::unique_ptr<byte[], PODAlignedDeletor> _pkt;
        std::shared_ptr<Marker> _marker;

        TexturePitches  _pitches;

        static void CALLBACK CompletionRoutine(
            DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered,
            LPOVERLAPPED lpOverlapped );
    };

    void* FileDataSource::GetData(SubResource subRes)
    {
        assert(subRes == 0);
        return _pkt.get();
    }

    size_t FileDataSource::GetDataSize(SubResource subRes) const           { assert(subRes == 0); return _dataSize; }
    TexturePitches FileDataSource::GetPitches(SubResource subRes) const    { assert(subRes == 0); return _pitches; }

    void CALLBACK FileDataSource::CompletionRoutine(
        DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered,
        LPOVERLAPPED lpOverlapped )
    {
        auto* o = (SpecialOverlapped*)lpOverlapped;
        assert(o && o->_returnPointer && o->_returnPointer->_marker);
        assert(o->_returnPointer->_marker->GetState() == Assets::AssetState::Pending);

        if (dwErrorCode == ERROR_SUCCESS) {
                // Transfer complete.
                // We don't have to do any extra processing right now. Just mark the asset as ready
            o->_returnPointer->_marker->SetState(Assets::AssetState::Invalid);
        } else {
                // failed somehow... this is now an invalid resource
            o->_returnPointer->_marker->SetState(Assets::AssetState::Invalid);
        }

            // we can reset the "_returnPointer", which will also decrease the reference
            // count on the FileDataSource object
        o->_returnPointer.reset();
    }

    auto FileDataSource::BeginBackgroundLoad() -> std::shared_ptr < Marker >
    {
        assert(!_marker);
        assert(_fileHandle && _fileHandle != INVALID_HANDLE_VALUE);

        // start the read operation now (it will happen asynchronously)
        //
        // We allocate the buffer here, to remove malloc costs from the caller thread

        _pkt.reset((byte*)XlMemAlign(_dataSize, 16));
        XlSetMemory(&_overlappedStatus._internal, 0, sizeof(_overlappedStatus._internal));
        _overlappedStatus._internal.Pointer = (void*)_offset;
        _overlappedStatus._returnPointer = this;

        auto result = ReadFileEx(
            _fileHandle, _pkt.get(), (DWORD)_dataSize, 
            &_overlappedStatus._internal, &CompletionRoutine);

        _marker = std::make_shared<Marker>();
        if (!result) {
            auto lastError = GetLastError();
            (void)lastError;

            _overlappedStatus._returnPointer.reset();
            _marker->SetState(Assets::AssetState::Invalid);
        }
        return _marker;
    }

    FileDataSource::FileDataSource(const void* fileHandle, size_t offset, size_t dataSize, TexturePitches pitches)
    {
        assert(dataSize);
        assert(fileHandle != INVALID_HANDLE_VALUE);

            //  duplicate the file handle so we get our own reference count on this
            //  file object.
        ::DuplicateHandle(
            GetCurrentProcess(), (HANDLE)fileHandle, GetCurrentProcess(),
            &_fileHandle, 0, FALSE, DUPLICATE_SAME_ACCESS);

        _dataSize = dataSize;
        _pitches = pitches;
        _offset = offset;
    }

    FileDataSource::~FileDataSource()
    {
        if (_fileHandle && _fileHandle!=INVALID_HANDLE_VALUE) {
            CloseHandle(_fileHandle);
        }
    }

    intrusive_ptr<DataPacket> CreateFileDataSource(const void* fileHandle, size_t offset, size_t dataSize, TexturePitches pitches)
    {
        return make_intrusive<FileDataSource>(fileHandle, offset, dataSize, pitches);
    }

}
