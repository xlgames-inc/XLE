// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DataPacket.h"
#include "PlatformInterface.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/ResourceUtils.h"
#include "../Assets/IFileSystem.h"
#include "../OSServices/Log.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../Utility/Threading/CompletionThreadPool.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Conversion.h"
#include "../Utility/StringUtils.h"
#include "../OSServices/RawFS.h"
#include <queue>
#include <thread>

#include "../Foreign/DirectXTex/DirectXTex/DirectXTex.h"

// Important character set note!
// We're using some "wchar_t" conversions in this file. This is because we're using some Windows API functions that take WCHAR strings
// (indirectly via the DirectXTex library).
// However, Windows should technically expect UTF16 (not UCS2) encoding for WCHAR. Maybe in our usage patterns it's not a big
// deal... But we should be careful!

namespace BufferUploads
{

    class BasicRawDataPacket : public DataPacket
    {
    public:
        virtual void* GetData(SubResourceId subRes = {});
        virtual size_t GetDataSize(SubResourceId subRes = {}) const;
        virtual TexturePitches GetPitches(SubResourceId subRes = {}) const;
        virtual std::shared_ptr<Marker> BeginBackgroundLoad();

        BasicRawDataPacket(size_t dataSize, const void* data = nullptr, TexturePitches pitches = TexturePitches());
        virtual ~BasicRawDataPacket();
    protected:
        std::unique_ptr<uint8_t, PODAlignedDeletor> _data; 
        size_t _dataSize;
        TexturePitches _pitches;

        BasicRawDataPacket(const BasicRawDataPacket&);
        BasicRawDataPacket& operator=(const BasicRawDataPacket&);
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    BasicRawDataPacket::BasicRawDataPacket(size_t dataSize, const void* data, TexturePitches pitches)
    : _dataSize(dataSize), _pitches(pitches)    
    {
        _data.reset((uint8_t*)XlMemAlign(dataSize, 16));

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

    void* BasicRawDataPacket::GetData(SubResourceId subRes)
    {
        assert(subRes._mip == 0 && subRes._arrayLayer == 0);
        return _data.get(); 
    }
    
    size_t BasicRawDataPacket::GetDataSize(SubResourceId subRes) const
    {
        assert(subRes._mip == 0 && subRes._arrayLayer == 0);
        return _dataSize; 
    }

    TexturePitches BasicRawDataPacket::GetPitches(SubResourceId subRes) const
    {
        assert(subRes._mip == 0 && subRes._arrayLayer == 0);
        return _pitches; 
    }

    auto BasicRawDataPacket::BeginBackgroundLoad() -> std::shared_ptr<Marker> { return nullptr; }

    intrusive_ptr<DataPacket> CreateBasicPacket(
        size_t dataSize, const void* data, TexturePitches rowAndSlicePitch)
    {
        return make_intrusive<BasicRawDataPacket>(dataSize, data, rowAndSlicePitch);
    }

    intrusive_ptr<DataPacket> CreateEmptyPacket(const ResourceDesc& desc)
    {
            // Create an empty packet of the appropriate size for the given desc
            // Linear buffers are simple, but textures need a little more detail...
        if (desc._type == ResourceDesc::Type::LinearBuffer) {
            auto size = RenderCore::ByteCount(desc);
            return make_intrusive<BasicRawDataPacket>(size, nullptr, TexturePitches{size, size});
        } else if (desc._type == ResourceDesc::Type::Texture) {
                //  currently not supporting textures with multiple mip-maps
                //  or multiple array slices
            assert(desc._textureDesc._mipCount <= 1);
            assert(desc._textureDesc._arrayCount <= 1);

            auto pitches = RenderCore::MakeTexturePitches(desc._textureDesc);
            return make_intrusive<BasicRawDataPacket>(pitches._slicePitch, nullptr, pitches);
        }

        return nullptr;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
            //      S T R E A M I N G   P A C K E T

    class FileDataSource : public DataPacket
    {
    public:
        virtual void*           GetData         (SubResourceId subRes);
        virtual size_t          GetDataSize     (SubResourceId subRes) const;
        virtual TexturePitches  GetPitches      (SubResourceId subRes) const;

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

    void* FileDataSource::GetData(SubResourceId subRes)
    {
        // assert(subRes == 0);
        return _pkt.get();
    }

    size_t FileDataSource::GetDataSize(SubResourceId subRes) const           { /*assert(subRes == 0);*/ return _dataSize; }
    TexturePitches FileDataSource::GetPitches(SubResourceId subRes) const    { /*assert(subRes == 0);*/ return _pitches; }

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
        ConsoleRig::GlobalServices::GetInstance().GetShortTaskThreadPool().Enqueue(
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

    class DirectXTextureLibraryDataPacket : public DataPacket
    {
    public:
        void*           GetData         (SubResourceId subRes) override;
        size_t          GetDataSize     (SubResourceId subRes) const override;
        TexturePitches  GetPitches      (SubResourceId subRes) const override;
		virtual ResourceDesc		GetDesc			() const override;

        std::shared_ptr<Marker>     BeginBackgroundLoad() override;

        DirectXTextureLibraryDataPacket(
            StringSection<::Assets::ResChar> filename,
            TextureLoadFlags::BitField flags);
        ~DirectXTextureLibraryDataPacket();
    private:
        DirectX::ScratchImage _image;
        DirectX::TexMetadata _texMetadata;
		ResourceDesc _desc;
    };

    void* DirectXTextureLibraryDataPacket::GetData(SubResourceId subRes)
    {
        auto* image = _image.GetImage(subRes._mip, subRes._arrayLayer, 0);
        if (image) return image->pixels;
        return nullptr;
    }

    size_t DirectXTextureLibraryDataPacket::GetDataSize(SubResourceId subRes) const
    {
        auto* image = _image.GetImage(subRes._mip, subRes._arrayLayer, 0);
        if (image) return image->slicePitch;
        return 0;
    }

    TexturePitches DirectXTextureLibraryDataPacket::GetPitches(SubResourceId subRes) const
    {
        auto* image = _image.GetImage(subRes._mip, subRes._arrayLayer, 0);
        if (image) return TexturePitches{unsigned(image->rowPitch), unsigned(image->slicePitch)};
        return TexturePitches{};
    }

	ResourceDesc		DirectXTextureLibraryDataPacket::GetDesc			() const
	{
		return _desc;
	}

	auto DirectXTextureLibraryDataPacket::BeginBackgroundLoad() -> std::shared_ptr<Marker> { return nullptr; }

    enum class TexFmt
    {
        DDS, TGA, WIC, Unknown
    };

    static TexFmt GetTexFmt(StringSection<> filename)
    {
        auto ext = MakeFileNameSplitter(filename).Extension();
        if (ext.IsEmpty()) return TexFmt::Unknown;

        if (XlEqStringI(ext, "dds")) {
            return TexFmt::DDS;
        } else if (XlEqStringI(ext, "tga")) {
            return TexFmt::TGA;
		} else {
            return TexFmt::WIC;     // try "WIC" for anything else
        }
    }

    static RenderCore::TextureDesc BuildTextureDesc(const DirectX::TexMetadata& metadata)
    {
        RenderCore::TextureDesc desc = RenderCore::TextureDesc::Empty();
        
        desc._width = uint32_t(metadata.width);
        desc._height = uint32_t(metadata.height);
        desc._depth = uint32_t(metadata.depth);
        desc._arrayCount = uint8_t(metadata.arraySize);
        desc._mipCount = uint8_t(metadata.mipLevels);
        desc._samples = TextureSamples::Create();

            // we need to use a "typeless" format for any pixel formats that can
            // cast to to SRGB or linear versions. This allows the caller to use
            // both SRGB and linear ShaderResourceView(s).
            // But, we don't want to do this for all formats that can become typeless
            // because we need to retain that information on the resource. For example,
            // if we made R32_** formats typeless here, when we create the shader resource
            // view there would be no way to know if it was originally a R32_FLOAT, or a R32_UINT (etc)
        auto srcFormat = (RenderCore::Format)metadata.format;
        if (RenderCore::HasLinearAndSRGBFormats(srcFormat)) {
            desc._format = RenderCore::AsTypelessFormat(srcFormat);
        } else {
            desc._format = srcFormat;
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

    DirectXTextureLibraryDataPacket::DirectXTextureLibraryDataPacket(
        StringSection<> inputFilename,
        TextureLoadFlags::BitField flags)
    {
        using namespace DirectX;

            // the DirectXTex library is expecting us to call CoInitializeEx.
            // We need to call this in every thread that uses the DirectXTex library.
            //  ... it should be ok to call it multiple times in the same thread, so
            //      let's just call it every time.
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        HRESULT hresult = -1;

        XlZeroMemory(_texMetadata);
		auto fmt = GetTexFmt(inputFilename);

        // If we want to support loading textures from within archives, we need to use the 
        // file functions in ::Assets::MainFileSystem. We want to avoid doing too many copies
        // during the texture initialization process, so we'll use the MemoryMappedFile interface.
        const bool loadViaMainFileSystem = true;
        if (loadViaMainFileSystem) {
            if (fmt == TexFmt::DDS || fmt == TexFmt::TGA || fmt == TexFmt::WIC) {
                OSServices::MemoryMappedFile srcFile;
                auto ioResult = ::Assets::MainFileSystem::TryOpen(srcFile, inputFilename, 0ull, "r");
                if (ioResult == ::Assets::IFileSystem::IOReason::Success) {
                    if (fmt == TexFmt::DDS) {
                        hresult = LoadFromDDSMemory(srcFile.GetData().begin(), srcFile.GetSize(), DDS_FLAGS_NONE, &_texMetadata, _image);
                    } else if (fmt == TexFmt::TGA) {
                        hresult = LoadFromTGAMemory(srcFile.GetData().begin(), srcFile.GetSize(), &_texMetadata, _image);
                    } else if (fmt == TexFmt::WIC) {
                        hresult = LoadFromWICMemory(srcFile.GetData().begin(), srcFile.GetSize(), WIC_FLAGS_NONE, &_texMetadata, _image);
                    } else {
                        Log(Warning) << "Texture format not apparent from filename (" << inputFilename.AsString() << ")" << std::endl;
                    }
                }
            } 
        } else {
            // LoadFromDDSFile, etc, takes LPCWSTR for the filename
            // this is oriented around the windows long char encoding scheme; and isn't particularly cross platform
			auto ucs2Filename = Conversion::Convert<std::basic_string<wchar_t>>(inputFilename.Cast<utf8>());
            if (fmt == TexFmt::DDS) {
                hresult = LoadFromDDSFile(ucs2Filename.c_str(), DDS_FLAGS_NONE, &_texMetadata, _image);
            } else if (fmt == TexFmt::TGA) {
                hresult = LoadFromTGAFile(ucs2Filename.c_str(), &_texMetadata, _image);
            } else if (fmt == TexFmt::WIC) {
                hresult = LoadFromWICFile(ucs2Filename.c_str(), WIC_FLAGS_NONE, &_texMetadata, _image);
            } else {
                Log(Warning) << "Texture format not apparent from filename (" << inputFilename.AsString() << ")" << std::endl;
            }
        }

        if (SUCCEEDED(hresult)) {
			auto loadedDDSFormat = fmt == TexFmt::DDS; 

            _desc._type = ResourceDesc::Type::Texture;
            _desc._textureDesc = BuildTextureDesc(_texMetadata);

            // note --	When loading from a .dds file, never generate the mipmaps. Typically we want mipmaps to be
            //			pre-generated and stored on disk. If we come across a dds file without mipmaps, we'll assume
            //			that it was intended to be that way.
            if (   (_texMetadata.mipLevels <= 1) && (_texMetadata.arraySize <= 1) 
                && (flags & TextureLoadFlags::GenerateMipmaps) && !loadedDDSFormat) {

                Log(Verbose) << "Building mipmaps for texture: " << inputFilename.AsString();
                DirectX::ScratchImage newImage;
                auto mipmapHresult = GenerateMipMaps(*_image.GetImage(0,0,0), (DWORD)TEX_FILTER_DEFAULT, 0, newImage);
                if (SUCCEEDED(mipmapHresult)) {
                    _image = std::move(newImage);
                    _desc._textureDesc._mipCount = uint8_t(_image.GetMetadata().mipLevels);
                } else {
                    Log(Warning) << "Failed while building mip-maps for texture: " << inputFilename.AsString() << std::endl;
                }
            }
        } else {
			_desc._type = ResourceDesc::Type::Unknown;
		}
    }

    DirectXTextureLibraryDataPacket::~DirectXTextureLibraryDataPacket() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class StreamingTexture : public DataPacket
    {
    public:
        virtual void*           GetData         (SubResourceId subRes) override;
        virtual size_t          GetDataSize     (SubResourceId subRes) const override;
        virtual TexturePitches  GetPitches      (SubResourceId subRes) const override;
		virtual ResourceDesc		GetDesc			() const override;

        virtual std::shared_ptr<Marker>     BeginBackgroundLoad() override;

        StreamingTexture(
			IteratorRange<const TexturePlugin*> plugins,
            StringSection<::Assets::ResChar> filename,
            TextureLoadFlags::BitField flags);
        virtual ~StreamingTexture();

    protected:
        intrusive_ptr<DataPacket> _realPacket;
        std::shared_ptr<Marker> _marker;

        std::basic_string<::Assets::ResChar> _filename;
        TextureLoadFlags::BitField _flags;
		std::vector<TexturePlugin> _plugins;
    };

    void*           StreamingTexture::GetData         (SubResourceId subRes)
    {
        if (_realPacket)
            return _realPacket->GetData(subRes);
        return nullptr;
    }

    size_t          StreamingTexture::GetDataSize     (SubResourceId subRes) const
    {
        if (_realPacket)
            return _realPacket->GetDataSize(subRes);
        return 0;
    }

    TexturePitches  StreamingTexture::GetPitches      (SubResourceId subRes) const
    {
        if (_realPacket)
            return _realPacket->GetPitches(subRes);
        return {};
    }

	ResourceDesc		StreamingTexture::GetDesc		() const
	{
		if (_realPacket)
            return _realPacket->GetDesc();
        return {};
	}

    auto StreamingTexture::BeginBackgroundLoad() -> std::shared_ptr < Marker >
    {
        assert(!_marker);

        _marker = std::make_shared<Marker>();
        intrusive_ptr<StreamingTexture> strongThis = this;      // hold a reference while the background operation is occurring

		// Managing the async operations here is a little bit awkward; but it works...
        ConsoleRig::GlobalServices::GetInstance().GetLongTaskThreadPool().Enqueue(
            [strongThis]()
            {
				std::basic_string<::Assets::ResChar> filename = strongThis->_filename;

				// replace semicolon dividers with null chars
				for (auto i=filename.begin(); i!=filename.end(); ++i)
					if (*i == L';') *i = L'\0';
        
				// The filename can actually contain multiple alternatives. We're going
				// to test each one until we find one that works. Scan forward until we
				// find 2 null characters in a row, or the end of the array
				auto fIterator = filename.begin();
				auto iend = filename.end();
				while (fIterator < iend) {

					auto partEnd = fIterator;
					while (partEnd < iend && *partEnd != L'\0') ++partEnd;

					TexturePlugin* plugin = nullptr;
					for (auto& p:strongThis->_plugins) {
						if (std::regex_match(fIterator, partEnd, p._filenameMatcher)) {
							plugin = &p;
						}
					}

					if (plugin) {
						strongThis->_realPacket = plugin->_loader(MakeStringSection(fIterator, partEnd), strongThis->_flags);
					} else {
						// drop back to default loading method
						strongThis->_realPacket = make_intrusive<DirectXTextureLibraryDataPacket>(MakeStringSection(fIterator, partEnd), strongThis->_flags);
					}

					if (strongThis->_realPacket->GetDataSize() != 0)
						break;

					fIterator = partEnd;
					if (fIterator < iend) ++fIterator;	// skip over this null, and there might be another filename
				}

				if (strongThis->_realPacket->GetDataSize() != 0) {
					strongThis->_marker->SetState(Assets::AssetState::Ready);
				} else {
					strongThis->_marker->SetState(Assets::AssetState::Invalid);
				}
            });
        
        return _marker;
    }

    StreamingTexture::StreamingTexture(
		IteratorRange<const TexturePlugin*> plugins,
        StringSection<::Assets::ResChar> filename,
        TextureLoadFlags::BitField flags)
    : _flags(flags), _filename(filename.begin(), filename.end())
	, _plugins(plugins.begin(), plugins.end())
    {}

    StreamingTexture::~StreamingTexture()
    {}

    intrusive_ptr<DataPacket> CreateStreamingTextureSource(
		IteratorRange<const TexturePlugin*> plugins,
        StringSection<::Assets::ResChar> filename, TextureLoadFlags::BitField flags)
    {
        return make_intrusive<StreamingTexture>(plugins, filename, flags);
    }

    RenderCore::TextureDesc LoadTextureFormat(StringSection<::Assets::ResChar> filename)
    {
        auto fmt = GetTexFmt(filename);
                
        using namespace DirectX;
        TexMetadata metadata;

        HRESULT hresult = -1;
		wchar_t wfilename[MaxPath];
        Conversion::Convert(wfilename, dimof(wfilename), filename.begin(), filename.end());

        if (fmt == TexFmt::DDS) {
            hresult = GetMetadataFromDDSFile((const wchar_t*)wfilename, DDS_FLAGS_NONE, metadata);
        } else if (fmt == TexFmt::TGA) {
            hresult = GetMetadataFromTGAFile((const wchar_t*)wfilename, metadata);
        } else if (fmt == TexFmt::WIC) {
            hresult = GetMetadataFromWICFile((const wchar_t*)wfilename, WIC_FLAGS_NONE, metadata);
        } else {
            Log(Warning) << "Texture format not apparent from filename (" << filename.AsString().c_str() << ")" << std::endl;
        }

        if (SUCCEEDED(hresult)) {
            return BuildTextureDesc(metadata);
        }

        return RenderCore::TextureDesc::Empty();
    }

}
