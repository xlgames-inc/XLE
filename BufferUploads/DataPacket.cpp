// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DataPacket.h"
#include "PlatformInterface.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../Utility/Threading/CompletionThreadPool.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Conversion.h"
#include "../Utility/StringUtils.h"
#include <queue>
#include <thread>

#include "../Foreign/DirectXTex/DirectXTex/DirectXTex.h"

// Important character set note!
// We're using some "ucs2" conversions in this file. This is because we're using some Windows API functions that take WCHAR strings
// (indirectly via the DirectXTex library).
// However, Windows should technically expect UTF16 (not UCS2) encoding for WCHAR. Maybe in our usage patterns it's not a big
// deal... But we should be careful!

namespace BufferUploads
{

///////////////////////////////////////////////////////////////////////////////////////////////////

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
            LPOVERLAPPED lpOverlapped);
    };

    void* FileDataSource::GetData(SubResource subRes)
    {
        // assert(subRes == 0);
        return _pkt.get();
    }

    size_t FileDataSource::GetDataSize(SubResource subRes) const           { /*assert(subRes == 0);*/ return _dataSize; }
    TexturePitches FileDataSource::GetPitches(SubResource subRes) const    { /*assert(subRes == 0);*/ return _pitches; }

    void CALLBACK FileDataSource::CompletionRoutine(
        DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered,
        LPOVERLAPPED lpOverlapped)
    {
        auto* o = (SpecialOverlapped*)lpOverlapped;
        assert(o && o->_returnPointer && o->_returnPointer->_marker);
        assert(o->_returnPointer->_marker->GetAssetState() == Assets::AssetState::Pending);

            // We don't have to do any extra processing right now. Just mark the asset as ready
            // or invalid, based on the result...
        o->_returnPointer->_marker->SetState(
            (dwErrorCode == ERROR_SUCCESS) ? Assets::AssetState::Ready : Assets::AssetState::Invalid);

            // we can reset the "_returnPointer", which will also decrease the reference
            // count on the FileDataSource object
        o->_returnPointer.reset();
    }

    auto FileDataSource::BeginBackgroundLoad() -> std::shared_ptr < Marker >
    {
        assert(!_marker);
        assert(_fileHandle && _fileHandle != INVALID_HANDLE_VALUE);

        // Queue read operation begin (it will happen asynchronously)...

        _marker = std::make_shared<Marker>();
        
        XlSetMemory(&_overlappedStatus._internal, 0, sizeof(_overlappedStatus._internal));
        _overlappedStatus._internal.Pointer = (void*)_offset;
        _overlappedStatus._returnPointer = this;

        auto* o = &_overlappedStatus;
            // this should be a very quick operation -- it might be best to put it in a
            // separate thread pool from the long operations
        ConsoleRig::GlobalServices::GetShortTaskThreadPool().Enqueue(
            [o]()
            {
                auto* pkt = o->_returnPointer.get();
                assert(pkt);

                    // We allocate the buffer here, to remove malloc costs from the caller thread
                pkt->_pkt.reset((byte*)XlMemAlign(pkt->_dataSize, 16));

                auto result = ReadFileEx(
                    pkt->_fileHandle, pkt->_pkt.get(), (DWORD)pkt->_dataSize, 
                    &pkt->_overlappedStatus._internal, &CompletionRoutine);

                if (!result) {
                    auto lastError = GetLastError();
                    (void)lastError;

                    pkt->_marker->SetState(Assets::AssetState::Invalid);
                    o->_returnPointer.reset();
                }
            });
        
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

///////////////////////////////////////////////////////////////////////////////////////////////////

    class StreamingTexture : public DataPacket
    {
    public:
        virtual void*           GetData         (SubResource subRes);
        virtual size_t          GetDataSize     (SubResource subRes) const;
        virtual TexturePitches  GetPitches      (SubResource subRes) const;

        virtual std::shared_ptr<Marker>     BeginBackgroundLoad();

        StreamingTexture(
            const ::Assets::ResChar filename[], const ::Assets::ResChar filenameEnd[],
            TextureLoadFlags::BitField flags);
        virtual ~StreamingTexture();

    protected:
        wchar_t _filename[MaxPath];
        
        DirectX::ScratchImage _image;
        DirectX::TexMetadata _texMetadata;

        intrusive_ptr<StreamingTexture> _returnPointer;
        std::shared_ptr<Marker> _marker;

        TextureLoadFlags::BitField _flags;
    };

    void* StreamingTexture::GetData(SubResource subRes)
    {
        auto arrayIndex = subRes >> 16u, mip = subRes & 0xffffu;
        auto* image = _image.GetImage(mip, arrayIndex, 0);
        if (image) return image->pixels;
        return nullptr;
    }

    size_t StreamingTexture::GetDataSize(SubResource subRes) const
    {
        auto arrayIndex = subRes >> 16u, mip = subRes & 0xffffu;
        auto* image = _image.GetImage(mip, arrayIndex, 0);
        if (image) return image->slicePitch;
        return 0;
    }

    TexturePitches StreamingTexture::GetPitches(SubResource subRes) const
    {
        auto arrayIndex = subRes >> 16u, mip = subRes & 0xffffu;
        auto* image = _image.GetImage(mip, arrayIndex, 0);
        if (image) return TexturePitches(unsigned(image->rowPitch), unsigned(image->slicePitch));
        return TexturePitches();
    }

    enum class TexFmt
    {
        DDS, TGA, WIC, Unknown
    };

    static TexFmt GetTexFmt(const ucs2 filename[])
    {
        auto* ext = XlExtension(filename);
        if (!ext) return TexFmt::Unknown;

        if (!XlCompareStringI(ext, (const ucs2*)L"dds")) {
            return TexFmt::DDS;
        } else if (ext && !XlCompareStringI(ext, (const ucs2*)L"tga")) {
            return TexFmt::TGA;
        } else {
            return TexFmt::WIC;     // try "WIC" for anything else
        }
    }

    static TextureDesc BuildTextureDesc(const DirectX::TexMetadata& metadata)
    {
        TextureDesc desc = TextureDesc::Empty();
        
        desc._width = uint32(metadata.width);
        desc._height = uint32(metadata.height);
        desc._depth = uint32(metadata.depth);
        desc._arrayCount = uint8(metadata.arraySize);
        desc._mipCount = uint8(metadata.mipLevels);
        desc._samples = TextureSamples();

            // we need to use a "typeless" format for any pixel formats that can
            // cast to to SRGB or linear versions. This allows the caller to use
            // both SRGB and linear ShaderResourceView(s).
            // But, we don't want to do this for all formats that can become typeless
            // because we need to retain that information on the resource. For example,
            // if we made R32_** formats typeless here, when we create the shader resource
            // view there would be no way to know if it was originally a R32_FLOAT, or a R32_UINT (etc)
        auto srcFormat = (RenderCore::Metal::NativeFormat::Enum)metadata.format;
        if (RenderCore::Metal::HasLinearAndSRGBFormats(srcFormat)) {
            desc._nativePixelFormat = (unsigned)RenderCore::Metal::AsTypelessFormat(srcFormat);
        } else {
            desc._nativePixelFormat = (unsigned)srcFormat;
        }

        using namespace DirectX;
        switch (metadata.dimension) {
        case TEX_DIMENSION_TEXTURE1D: desc._dimensionality = TextureDesc::Dimensionality::T1D; break;
        default:
        case TEX_DIMENSION_TEXTURE2D: 
            if (metadata.miscFlags & TEX_MISC_TEXTURECUBE)
                desc._dimensionality = TextureDesc::Dimensionality::CubeMap; 
            else
                desc._dimensionality = TextureDesc::Dimensionality::T2D; 
            break;
        case TEX_DIMENSION_TEXTURE3D: desc._dimensionality = TextureDesc::Dimensionality::T3D; break;
        }
        if (metadata.IsCubemap())
            desc._dimensionality = TextureDesc::Dimensionality::CubeMap;
        return desc;
    }

    auto StreamingTexture::BeginBackgroundLoad() -> std::shared_ptr < Marker >
    {
        assert(!_marker && !_returnPointer);

        _marker = std::make_shared<Marker>();
        _returnPointer = this;      // hold a reference while the background operation is occurring

        ConsoleRig::GlobalServices::GetLongTaskThreadPool().Enqueue(
            [this]()
            {
                using namespace DirectX;

                    // the DirectXTex library is expecting us to call CoInitializeEx.
                    // We need to call this in every thread that uses the DirectXTex library.
                    //  ... it should be ok to call it multiple times in the same thread, so
                    //      let's just call it every time.
                CoInitializeEx(nullptr, COINIT_MULTITHREADED);

                    // We want to release the reference in this->_returnPointer at the
                    // end of this function, no matter what happens (successful return,
                    // failure return, or exception). Let's move the reference into a 
                    // local variable to ensure that happens.
                decltype(this->_returnPointer) releaseReference = std::move(this->_returnPointer);

                HRESULT hresult = -1;
				bool loadedDDSFormat = false;
				
					// The filename can actually contain multiple alternatives. We're going
					// to test each one until we find one that works. Scan forward until we
					// find 2 null characters in a row, or the end of the array
				const auto* filename = this->_filename;
				const auto *iend = &this->_filename[dimof(this->_filename)];
				while (*filename && filename < iend) {
					auto fmt = GetTexFmt((const ucs2*)filename);
                
					if (fmt == TexFmt::DDS) {
						hresult = LoadFromDDSFile(filename, DDS_FLAGS_NONE, &_texMetadata, _image);
					} else if (fmt == TexFmt::TGA) {
						hresult = LoadFromTGAFile(filename, &_texMetadata, _image);
					} else if (fmt == TexFmt::WIC) {
						hresult = LoadFromWICFile(filename, WIC_FLAGS_NONE, &_texMetadata, _image);
					} else {
						LogWarning << "Texture format not apparent from filename (" << filename << ")";
					}

					if (SUCCEEDED(hresult)) { loadedDDSFormat = fmt == TexFmt::DDS; break; }

					while (filename < iend && *filename != L'\0') ++filename;
					if (filename < iend) ++filename;	// skip over this null, and there might be another filename
				}

                if (SUCCEEDED(hresult)) {
                    auto& desc = this->_marker->_desc;
                    desc._type = BufferDesc::Type::Texture;
                    desc._textureDesc = BuildTextureDesc(_texMetadata);

					// note --	When loading from a .dds file, never generate the mipmaps. Typically we want mipmaps to be
					//			pre-generated and stored on disk. If we come across a dds file without mipmaps, we'll assume
					//			that it was intended to be that way.
                    if (   (this->_texMetadata.mipLevels <= 1) && (this->_texMetadata.arraySize <= 1) 
                        && (this->_flags & TextureLoadFlags::GenerateMipmaps) && !loadedDDSFormat) {

						LogInfo << "Building mipmaps for texture: " << filename;
                        DirectX::ScratchImage newImage;
                        auto mipmapHresult = GenerateMipMaps(*this->_image.GetImage(0,0,0), (DWORD)TEX_FILTER_DEFAULT, 0, newImage);
                        if (SUCCEEDED(mipmapHresult)) {
                            this->_image = std::move(newImage);
                            desc._textureDesc._mipCount = uint8(this->_image.GetMetadata().mipLevels);
                        } else {
                            LogWarning << "Failed while building mip-maps for texture: " << filename;
                        }
                    }

                    if (SUCCEEDED(hresult)) {
                        this->_marker->SetState(Assets::AssetState::Ready);
                        return;
                    }
                }

                this->_marker->SetState(Assets::AssetState::Invalid);
            });
        
        return _marker;
    }

    StreamingTexture::StreamingTexture(
        const ::Assets::ResChar filename[], const ::Assets::ResChar filenameEnd[],
        TextureLoadFlags::BitField flags)
    : _flags(flags)
    {
        XlZeroMemory(_texMetadata);
		XlZeroMemory(_filename);
        Conversion::Convert(_filename, dimof(_filename), filename, filenameEnd);

		// replace semicolon dividers with null chars
		for (auto* i=_filename; *i; ++i)
			if (*i == L';') *i = L'\0';
    }

    StreamingTexture::~StreamingTexture()
    {}

    intrusive_ptr<DataPacket> CreateStreamingTextureSource(
        StringSection<::Assets::ResChar> filename, TextureLoadFlags::BitField flags)
    {
        return make_intrusive<StreamingTexture>(filename.begin(), filename.end(), flags);
    }

    TextureDesc LoadTextureFormat(StringSection<::Assets::ResChar> filename)
    {
        ucs2 wfilename[MaxPath];
        Conversion::Convert(wfilename, dimof(wfilename), filename.begin(), filename.end());

        auto fmt = GetTexFmt(wfilename);
                
        using namespace DirectX;
        TexMetadata metadata;

        HRESULT hresult = -1;
        if (fmt == TexFmt::DDS) {
            hresult = GetMetadataFromDDSFile((const wchar_t*)wfilename, DDS_FLAGS_NONE, metadata);
        } else if (fmt == TexFmt::TGA) {
            hresult = GetMetadataFromTGAFile((const wchar_t*)wfilename, metadata);
        } else if (fmt == TexFmt::WIC) {
            hresult = GetMetadataFromWICFile((const wchar_t*)wfilename, WIC_FLAGS_NONE, metadata);
        } else {
            LogWarning << "Texture format not apparent from filename (" << filename.AsString().c_str() << ")";
        }

        if (SUCCEEDED(hresult)) {
            return BuildTextureDesc(metadata);
        }

        return TextureDesc::Empty();
    }

}
