// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TextureLoaders.h"
#include "../../Assets/IFileSystem.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../OSServices/RawFS.h"
#include "../../OSServices/Log.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/Threading/CompletionThreadPool.h"
#include "../../Utility/Streams/PathUtils.h"
#include <memory>

#define ENABLE_DXTEX 1
#if ENABLE_DXTEX
	#if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
		#include "../../OSServices/WinAPI/IncludeWindows.h"	// get in before DirectXTex includes it
	#endif
	#define DeleteFile DeleteFileA
	#include "../../Foreign/DirectXTex/DirectXTex/DirectXTex.h"
	#include "../../Foreign/DirectXTex/DirectXTex/DDS.h"
	#include "../../Foreign/DirectXTex/DirectXTex/DirectXTexP.h"
	#undef DeleteFile
#endif

namespace RenderCore { namespace Techniques
{

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

	class DDSDataSource : public BufferUploads::IAsyncDataSource, public std::enable_shared_from_this<DDSDataSource>
	{
	public:
		virtual std::future<ResourceDesc> GetDesc() override
		{
			struct Captures
			{
				std::promise<ResourceDesc> _promise;
			};
			auto cap = std::make_shared<Captures>();
			auto result = cap->_promise.get_future();
			ConsoleRig::GlobalServices::GetInstance().GetShortTaskThreadPool().EnqueueBasic(
				[weakThis{weak_from_this()}, captures=std::move(cap)]() {
					try
					{
						auto that = weakThis.lock();
						if (!that)
							Throw(std::runtime_error("Data source has expired"));

						ScopedLock(that->_lock);

						if (!that->_hasReadMetadata) {
							if (!that->_file.IsGood())
								that->_file = ::Assets::MainFileSystem::OpenMemoryMappedFile(that->_filename, 0ull, "r");
						
							auto hres = DirectX::GetMetadataFromDDSMemory(that->_file.GetData().begin(), that->_file.GetSize(), DirectX::DDS_FLAGS_NONE, that->_texMetadata);
							if (!SUCCEEDED(hres))
								Throw(std::runtime_error("Failed while attempting reading header from DDS file (" + that->_filename + ")"));

							auto textureDesc = BuildTextureDesc(that->_texMetadata);
							that->_resourceDesc = CreateDesc(0, 0, 0, textureDesc, that->_filename);
							that->_hasReadMetadata = true;
						}

						captures->_promise.set_value(that->_resourceDesc);
					} catch(...) {
						captures->_promise.set_exception(std::current_exception());
					}
				});

			return result;
		}

		virtual std::future<void> PrepareData(IteratorRange<const SubResource*> subResources) override
		{
			struct Captures
			{
				std::promise<void> _promise;
				std::vector<SubResource> _subResources;
				Captures(IteratorRange<const SubResource*> subResources) :_subResources(subResources.begin(), subResources.end()) {}
			};
			auto cap = std::make_shared<Captures>(subResources);
			auto result = cap->_promise.get_future();
			ConsoleRig::GlobalServices::GetInstance().GetShortTaskThreadPool().EnqueueBasic(
				[weakThis{weak_from_this()}, captures = std::move(cap)]() mutable {
					try
					{
						auto that = weakThis.lock();
						if (!that)
							Throw(std::runtime_error("Data source has expired"));

						ScopedLock(that->_lock);
						if (!that->_file.IsGood())
							that->_file = ::Assets::MainFileSystem::OpenMemoryMappedFile(that->_filename, 0ull, "r");

						assert(that->_hasReadMetadata);

						// We need to get the image data from the file and copy it into the locations requested
						// The normal usage of the DirectXTex library is to use LoadFromDDSMemory() and 
						// construct a series of DirectX::ScatchImage objects. However, that would result in an
						// extra copy (ie, copy mapped file -> ScatchImage -> staging texture output)
						// We can skip that copy if we use the internal DirectXTex library functions directly

						if (that->_texMetadata.dimension == DirectX::TEX_DIMENSION_TEXTURE3D)
							Throw(std::runtime_error("3D DSS textures encountered while reading (" + that->_filename + "). Reading this type of texture is not supported."));

						size_t pixelSize, nimages;
						if (!DirectX::_DetermineImageArray(that->_texMetadata, DirectX::CP_FLAGS_NONE, nimages, pixelSize))
							Throw(std::runtime_error("Could not determine image offsets when loading DDS file (" + that->_filename + "). This file may be truncated?"));

						size_t offset = sizeof(uint32_t) + sizeof(DirectX::DDS_HEADER);
						auto* pHeader = reinterpret_cast<const DirectX::DDS_HEADER*>(PtrAdd(that->_file.GetData().begin(), sizeof(uint32_t)));
						if ((pHeader->ddspf.flags & DDS_FOURCC) && (MAKEFOURCC('D', 'X', '1', '0') == pHeader->ddspf.fourCC))
							offset += sizeof(DirectX::DDS_HEADER_DXT10);
						void* pixels = PtrAdd(that->_file.GetData().begin(), offset);

						if ((pixelSize + offset) > that->_file.GetData().size())
							Throw(std::runtime_error("DDS file appears truncating when reading (" + that->_filename + ")"));

						DirectX::Image dximages[nimages];
						if (!_SetupImageArray(
							(uint8_t*)pixels,
							pixelSize,
							that->_texMetadata,
							DirectX::CP_FLAGS_NONE, dximages, nimages))
							Throw(std::runtime_error("Failure while reading images in DDS file (" + that->_filename + ")"));

						for (const auto& sr:captures->_subResources) {
							auto imageIdx = sr._id._arrayLayer * that->_texMetadata.mipLevels + sr._id._mip;
							if (imageIdx >= nimages)
								Throw(std::runtime_error("Invalid subresource encounted while reading DDS file (" + that->_filename + ")"));
							auto& image = dximages[imageIdx];
							TexturePitches expectedPitches {
								(unsigned)image.rowPitch,
								(unsigned)image.slicePitch,
								(unsigned)image.slicePitch
							};
							assert(expectedPitches._rowPitch == sr._pitches._rowPitch);
							assert(expectedPitches._slicePitch == sr._pitches._slicePitch);
							assert(expectedPitches._arrayPitch == sr._pitches._arrayPitch);
							assert(sr._destination.size() == (size_t)sr._pitches._slicePitch);
							std::memcpy(
								sr._destination.begin(),
								image.pixels,
								std::min(image.slicePitch, sr._destination.size()));
						}

						that->_file = {};		// close the file now, because we're probably done with it
						captures->_promise.set_value();
					} catch(...) {
						captures->_promise.set_exception(std::current_exception());
					}
				});
			return result;
		}

		DDSDataSource(const std::string& filename)
		: _filename(filename)
		{
			_hasReadMetadata = false;
		}
		~DDSDataSource() {}
	private:
		std::string _filename;

		std::mutex _lock;
		OSServices::MemoryMappedFile _file;
		DirectX::TexMetadata _texMetadata;
		RenderCore::ResourceDesc _resourceDesc;
		bool _hasReadMetadata = false;
	};

	std::function<TextureLoaderSignature> GetDDSTextureLoader()
	{
		// the DirectXTex library is expecting us to call CoInitializeEx.
		// We need to call this in every thread that uses the DirectXTex library.
		//  ... it should be ok to call it multiple times in the same thread, so
		//      let's just call it every time.
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		return [](StringSection<> filename, TextureLoaderFlags::BitField flags) -> std::shared_ptr<BufferUploads::IAsyncDataSource> {
			assert(!(flags & TextureLoaderFlags::GenerateMipmaps));
			return std::make_shared<DDSDataSource>(filename.AsString());
		};
	}

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

	class WICDataSource : public BufferUploads::IAsyncDataSource, public std::enable_shared_from_this<WICDataSource>
	{
	public:
		virtual std::future<ResourceDesc> GetDesc() override
		{
			struct Captures
			{
				std::promise<ResourceDesc> _promise;
			};
			auto cap = std::make_shared<Captures>();
			auto result = cap->_promise.get_future();
			ConsoleRig::GlobalServices::GetInstance().GetShortTaskThreadPool().EnqueueBasic(
				[weakThis{weak_from_this()}, captures=std::move(cap)]() {
					try
					{
						auto that = weakThis.lock();
						if (!that)
							Throw(std::runtime_error("Data source has expired"));

						ScopedLock(that->_lock);

						if (!that->_hasBeenInitialized) {
							auto file = ::Assets::MainFileSystem::OpenMemoryMappedFile(that->_filename, 0ull, "r");

							using namespace DirectX;
							auto fmt = GetTexFmt(that->_filename);
							HRESULT hresult = -1;
							if (fmt == TexFmt::DDS) {
								hresult = LoadFromDDSMemory(file.GetData().begin(), file.GetSize(), DDS_FLAGS_NONE, &that->_texMetadata, that->_image);
							} else if (fmt == TexFmt::TGA) {
								hresult = LoadFromTGAMemory(file.GetData().begin(), file.GetSize(), &that->_texMetadata, that->_image);
							} else {
								assert(fmt == TexFmt::WIC);
								hresult = LoadFromWICMemory(file.GetData().begin(), file.GetSize(), WIC_FLAGS_NONE, &that->_texMetadata, that->_image);
							}

							if (!SUCCEEDED(hresult))
								Throw(std::runtime_error("Failure while reading texture file (" + that->_filename + "). Check for corrupted data."));

							if (   (that->_texMetadata.mipLevels <= 1) && (that->_texMetadata.arraySize <= 1) 
								&& (that->_flags & TextureLoaderFlags::GenerateMipmaps) && fmt != TexFmt::DDS) {

								Log(Verbose) << "Building mipmaps for texture: " << that->_filename << std::endl;
								DirectX::ScratchImage newImage;
								auto mipmapHresult = GenerateMipMaps(*that->_image.GetImage(0,0,0), TEX_FILTER_DEFAULT, 0, newImage);
								if (!SUCCEEDED(mipmapHresult))
									Throw(std::runtime_error("Failed while building mip-maps for texture (" + that->_filename + ")"));

								that->_image = std::move(newImage);
								that->_texMetadata = that->_image.GetMetadata();
							}

							that->_hasBeenInitialized = true;
						}

						auto textureDesc = BuildTextureDesc(that->_texMetadata);
						auto resourceDesc = CreateDesc(0, 0, 0, textureDesc, that->_filename);
						captures->_promise.set_value(resourceDesc);
					} catch(...) {
						captures->_promise.set_exception(std::current_exception());
					}
				});

			return result;
		}

		virtual std::future<void> PrepareData(IteratorRange<const SubResource*> subResources) override
		{
			struct Captures
			{
				std::promise<void> _promise;
				std::vector<SubResource> _subResources;
				Captures(IteratorRange<const SubResource*> subResources) :_subResources(subResources.begin(), subResources.end()) {}
			};
			auto cap = std::make_shared<Captures>(subResources);
			auto result = cap->_promise.get_future();
			ConsoleRig::GlobalServices::GetInstance().GetShortTaskThreadPool().EnqueueBasic(
				[weakThis{weak_from_this()}, captures = std::move(cap)]() mutable {
					try
					{
						auto that = weakThis.lock();
						if (!that)
							Throw(std::runtime_error("Data source has expired"));

						ScopedLock(that->_lock);
						assert(that->_hasBeenInitialized);

						for (const auto& sr:captures->_subResources) {
							auto* image = that->_image.GetImage(sr._id._mip, sr._id._arrayLayer, 0);
							if (!image)
								continue;

							assert((unsigned)image->rowPitch == sr._pitches._rowPitch);
							assert((unsigned)image->slicePitch == sr._pitches._slicePitch);
							assert(sr._destination.size() == (size_t)sr._pitches._slicePitch);
							std::memcpy(
								sr._destination.begin(),
								image->pixels,
								std::min(image->slicePitch, sr._destination.size()));
						}
						
						captures->_promise.set_value();
					} catch(...) {
						captures->_promise.set_exception(std::current_exception());
					}
				});
			return result;
		}

		WICDataSource(const std::string& filename, TextureLoaderFlags::BitField flags)
		: _filename(filename)
		{
			_hasBeenInitialized = false;
		}
		~WICDataSource() {}
	private:
		std::string _filename;
		TextureLoaderFlags::BitField _flags;

		std::mutex _lock;
		DirectX::TexMetadata _texMetadata;
		DirectX::ScratchImage _image;
		bool _hasBeenInitialized = false;
	};

	std::function<TextureLoaderSignature> GetWICTextureLoader()
	{
		// the DirectXTex library is expecting us to call CoInitializeEx.
		// We need to call this in every thread that uses the DirectXTex library.
		//  ... it should be ok to call it multiple times in the same thread, so
		//      let's just call it every time.
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		return [](StringSection<> filename, TextureLoaderFlags::BitField flags) -> std::shared_ptr<BufferUploads::IAsyncDataSource> {
			return std::make_shared<WICDataSource>(filename.AsString(), flags);
		};
	}

	void RegisterTextureLoader(
		std::regex _filenameMatcher,
		std::function<TextureLoaderSignature>&& loader)
	{
		// ....
	}

}}
