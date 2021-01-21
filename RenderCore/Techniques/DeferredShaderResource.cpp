// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "DeferredShaderResource.h"
#include "Services.h"
#include "../Metal/TextureView.h"
#include "../Format.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../BufferUploads/ResourceLocator.h"
#include "../../Assets/Assets.h"
#include "../../Assets/AssetFuture.h"
#include "../../Assets/IntermediateAssets.h"	// (for MakeIntermediateName)
#include "../../Assets/IFileSystem.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/AssetServices.h"

#include "../../Utility/Streams/PathUtils.h"
#include "../../OSServices/RawFS.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/StreamDOM.h"
#include "../../Utility/ParameterBox.h"
#include "../../ConsoleRig/ResourceBox.h"

namespace RenderCore { namespace Techniques 
{
    using ResChar = ::Assets::ResChar;

    enum class SourceColorSpace { SRGB, Linear, Unspecified };

	class TextureMetaData
	{
	public:
		SourceColorSpace _colorSpace = SourceColorSpace::Unspecified;
		const ::Assets::DepValPtr&				GetDependencyValidation() const     { return _depVal; }

		TextureMetaData(
			InputStreamFormatter<utf8>& input, 
			const ::Assets::DirectorySearchRules&, 
			const ::Assets::DepValPtr& depVal);
	private:
		::Assets::DepValPtr _depVal;
	};

	TextureMetaData::TextureMetaData(
		InputStreamFormatter<utf8>& input, 
		const ::Assets::DirectorySearchRules&, 
		const ::Assets::DepValPtr& depVal)
	: _depVal(depVal)
	{
		StreamDOM<InputStreamFormatter<utf8>> dom(input);
        if (!dom.RootElement().children().empty()) {
            auto colorSpace = dom.RootElement().children().begin()->Attribute("colorSpace");
            if (colorSpace) {
                if (!XlCompareStringI(colorSpace.Value(), "srgb")) { _colorSpace = SourceColorSpace::SRGB; }
                else if (!XlCompareStringI(colorSpace.Value(), "linear")) { _colorSpace = SourceColorSpace::Linear; }
            }
        }
	}

    class DecodedInitializer
    {
    public:
        SourceColorSpace    _colSpaceRequestString;
        SourceColorSpace    _colSpaceDefault;
        bool                _generateMipmaps;

        DecodedInitializer(const FileNameSplitter<ResChar>& initializer);
    };

    DecodedInitializer::DecodedInitializer(const FileNameSplitter<ResChar>& initializer)
    {
        _generateMipmaps = true;
        _colSpaceRequestString = SourceColorSpace::Unspecified;
        _colSpaceDefault = SourceColorSpace::Unspecified;

        for (auto c:initializer.Parameters()) {
            if (c == 'l' || c == 'L') { _colSpaceRequestString = SourceColorSpace::Linear; }
            if (c == 's' || c == 'S') { _colSpaceRequestString = SourceColorSpace::SRGB; }
            if (c == 't' || c == 'T') { _generateMipmaps = false; }
        }

        if (_colSpaceRequestString == SourceColorSpace::Unspecified) {
            if (XlFindStringI(initializer.File(), "_ddn")) {
                _colSpaceDefault = SourceColorSpace::Linear;
            } else {
                _colSpaceDefault = SourceColorSpace::SRGB;
            }
        }
    }

	static bool CheckShadowingFile(const FileNameSplitter<::Assets::ResChar>& splitter)
	{
		return !XlEqStringI(splitter.Extension(), "dds")
			&& !std::find(splitter.FullFilename().begin(), splitter.FullFilename().end(), ';');
	}

	template<int Count>
		static void BuildRequestString(
			::Assets::ResChar (&buffer)[Count],
			const FileNameSplitter<::Assets::ResChar>& splitter)
	{
		auto& store = ::Assets::Services::GetAsyncMan().GetShadowingStore();
		store->MakeIntermediateName(
			buffer, Count, MakeStringSection(splitter.DriveAndPath().begin(), splitter.File().end()));
		XlCatString(buffer, Count, ".dds;");
		XlCatString(buffer, Count, splitter.AllExceptParameters());
	}

	class TransactionMonitor
	{
	public:
		BufferUploads::TransactionID GetId() const { return _id; }
		TransactionMonitor(BufferUploads::TransactionID id = ~BufferUploads::TransactionID(0)) : _id(id) {}
		~TransactionMonitor()
		{
			if (_id != ~BufferUploads::TransactionID(0)) {
				assert(Services::HasInstance());
				Services::GetBufferUploads().Transaction_End(_id);
			}
		}
		TransactionMonitor(TransactionMonitor&& moveFrom)
		: _id(moveFrom._id)
		{
			moveFrom._id = ~BufferUploads::TransactionID(0);
		}
		TransactionMonitor& operator=(TransactionMonitor&& moveFrom)
		{
			if (_id != ~BufferUploads::TransactionID(0)) {
				Services::GetBufferUploads().Transaction_End(_id);
				_id = ~BufferUploads::TransactionID(0);
			}
			std::swap(_id, moveFrom._id);
			return *this;
		}
	private:
		BufferUploads::TransactionID _id;
	};

	void DeferredShaderResource::ConstructToFuture(
		::Assets::AssetFuture<DeferredShaderResource>& future,
		StringSection<> initializer)
    {
            // parse initialiser for flags
		auto splitter = MakeFileNameSplitter(initializer);
        DecodedInitializer init(splitter);

		assert(!splitter.File().IsEmpty());

		std::shared_ptr<::Assets::AssetFuture<TextureMetaData>> metaDataFuture;

		::Assets::ResChar filename[MaxPath];
        if (init._colSpaceRequestString == SourceColorSpace::Unspecified) {
                // No color space explicitly requested. We need to calculate the default
                // color space for this texture...
                // Most textures should be in SRGB space. However, some texture represent
                // geometry details (or are lookup tables for shader calculations). These
                // need to be marked as linear space (so they don't go through the SRGB->Linear 
                // conversion.
                //
                // Some resources might have a little xml ".metadata" file attached. This 
                // can contain a setting that can tell us the intended source color format.
                //
                // Some textures use "_ddn" to mark them as normal maps... So we'll take this
                // as a hint that they are in linear space. 

                // trigger a load of the metadata file (which should proceed in the background)
            
            XlCopyString(filename, splitter.AllExceptParameters());
            XlCatString(filename, ".metadata");
			if (::Assets::MainFileSystem::TryGetDesc(filename)._state == ::Assets::FileDesc::State::Normal)
				metaDataFuture = ::Assets::MakeAsset<TextureMetaData>(filename);
        }

        using namespace BufferUploads;
        TextureLoadFlags::BitField flags = init._generateMipmaps ? TextureLoadFlags::GenerateMipmaps : 0;

		// We're going to check for the existance of a "shadowing" file first. We'll write onto "filename"
		// two names -- a possible shadowing file, and the original file as well. But don't do this for
		// DDS files. We'll assume they do not have a shadowing file.
		intrusive_ptr<DataPacket> pkt;
		const bool checkForShadowingFile = CheckShadowingFile(splitter);
		if (checkForShadowingFile) {
			BuildRequestString(filename, splitter);
			pkt = CreateStreamingTextureSource(RenderCore::Techniques::Services::GetInstance().GetTexturePlugins(), MakeStringSection(filename), flags);
		} else {
			pkt = CreateStreamingTextureSource(RenderCore::Techniques::Services::GetInstance().GetTexturePlugins(), splitter.AllExceptParameters(), flags);
		}

        auto transactionId = RenderCore::Techniques::Services::GetBufferUploads().Transaction_Begin(
            CreateDesc(
                BindFlag::ShaderResource,
                0, GPUAccess::Read,
                TextureDesc::Empty(), initializer),
            pkt.get());

		auto depVal = std::make_shared<::Assets::DependencyValidation>();
		::Assets::RegisterFileDependency(depVal, splitter.AllExceptParameters());

		if (transactionId == ~BufferUploads::TransactionID(0)) {
			future.SetInvalidAsset(depVal, ::Assets::AsBlob("Could not begin buffer uploads transaction"));
			return;
		}

		auto mon = std::make_shared<TransactionMonitor>(transactionId);

		std::string intializerStr = initializer.AsString();

        future.SetPollingFunction(
			[mon, metaDataFuture, depVal, init, intializerStr](::Assets::AssetFuture<DeferredShaderResource>& thatFuture) -> bool {
				auto& bu = RenderCore::Techniques::Services::GetBufferUploads();
				if (!bu.IsCompleted(mon->GetId())
					|| (metaDataFuture && metaDataFuture->GetAssetState() == ::Assets::AssetState::Pending))
					return true;

				auto locator = bu.GetResource(mon->GetId());
				*mon = {};	// release the transaction

				if (!locator || !locator->GetUnderlying()) {
					thatFuture.SetInvalidAsset(depVal, ::Assets::AsBlob("Buffer upload transaction completed, but with invalid resource"));
					return false;
				}

				auto desc = locator->GetUnderlying()->GetDesc();
				if (desc._type != RenderCore::ResourceDesc::Type::Texture) {
					thatFuture.SetInvalidAsset(depVal, ::Assets::AsBlob("Unexpected resource type returned from buffer uploads transaction"));
					return false;
				}

					// calculate the color space to use (resolving the defaults, request string and metadata)
				auto colSpace = SourceColorSpace::Unspecified;
				auto fmtComponentType = GetComponentType(desc._textureDesc._format);
				if (fmtComponentType == FormatComponentType::UNorm_SRGB) {
					colSpace = SourceColorSpace::SRGB;
				} else if (fmtComponentType != FormatComponentType::Typeless) {
					colSpace = SourceColorSpace::Linear;
				} else if (init._colSpaceRequestString != SourceColorSpace::Unspecified) {
					colSpace = init._colSpaceRequestString;
				} else if (metaDataFuture) {
					auto metaData = metaDataFuture->TryActualize();
					if (metaData) {
						if (metaData->_colorSpace != SourceColorSpace::Unspecified)
							colSpace = metaData->_colorSpace;
						::Assets::RegisterAssetDependency(depVal, metaData->GetDependencyValidation());
					}
				}

				if (colSpace == SourceColorSpace::Unspecified && init._colSpaceDefault != SourceColorSpace::Unspecified)
					colSpace = init._colSpaceDefault;

				auto format = desc._textureDesc._format;
				if (colSpace == SourceColorSpace::SRGB) format = AsSRGBFormat(format);
				else if (colSpace == SourceColorSpace::Linear) format = AsLinearFormat(format);

				auto finalAsset = std::make_shared<DeferredShaderResource>(
					Metal::ShaderResourceView(locator->GetUnderlying(), {format}),
					intializerStr,
					depVal);

				thatFuture.SetAsset(std::move(finalAsset), nullptr);
				return false;
			});
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static Format ResolveFormatImmediate(Format typelessFormat, const DecodedInitializer& init, const FileNameSplitter<ResChar>& splitter)
    {
        auto result = typelessFormat;
        if (HasLinearAndSRGBFormats(result)) {
            auto finalColSpace = init._colSpaceRequestString;
            if (finalColSpace == SourceColorSpace::Unspecified) {
                    // need to load the metadata file to get SRGB settings!
                ::Assets::ResChar metadataFile[MaxPath];
                XlCopyString(metadataFile, splitter.AllExceptParameters());
                XlCatString(metadataFile, ".metadata");

				auto res = ::Assets::MakeAsset<TextureMetaData>(metadataFile);
				res->StallWhilePending();
				auto actual = res->TryActualize();
				if (actual)
					finalColSpace = actual->_colorSpace;

                if (finalColSpace == SourceColorSpace::Unspecified)
                    finalColSpace = (init._colSpaceDefault != SourceColorSpace::Unspecified) ? init._colSpaceDefault : SourceColorSpace::SRGB;
            }

            if (finalColSpace == SourceColorSpace::SRGB) result = AsSRGBFormat(result);
            else if (finalColSpace == SourceColorSpace::Linear) result = AsLinearFormat(result);
        }
        return result;
    }

	Format DeferredShaderResource::LoadFormat(StringSection<::Assets::ResChar> initializer)
    {
		auto splitter = MakeFileNameSplitter(initializer);
        DecodedInitializer init(splitter);

		Format result;
		const bool checkForShadowingFile = CheckShadowingFile(splitter);
		if (checkForShadowingFile) {
			::Assets::ResChar filename[MaxPath];
			BuildRequestString(filename, splitter);
			result = BufferUploads::LoadTextureFormat(MakeStringSection(filename))._format;
		} else
			result = BufferUploads::LoadTextureFormat(splitter.AllExceptParameters())._format;

        return ResolveFormatImmediate(result, init, splitter);
    }

    Metal::ShaderResourceView DeferredShaderResource::LoadImmediately(StringSection<::Assets::ResChar> initializer)
    {
		auto splitter = MakeFileNameSplitter(initializer);
        DecodedInitializer init(splitter);

        using namespace BufferUploads;
        TextureLoadFlags::BitField flags = init._generateMipmaps ? TextureLoadFlags::GenerateMipmaps : 0;

		intrusive_ptr<DataPacket> pkt;
		const bool checkForShadowingFile = CheckShadowingFile(splitter);
		if (checkForShadowingFile) {
			::Assets::ResChar filename[MaxPath];
			BuildRequestString(filename, splitter);
			pkt = CreateStreamingTextureSource(RenderCore::Techniques::Services::GetInstance().GetTexturePlugins(), MakeStringSection(filename), flags);
		} else
			pkt = CreateStreamingTextureSource(RenderCore::Techniques::Services::GetInstance().GetTexturePlugins(), splitter.AllExceptParameters(), flags);

        auto result = Services::GetBufferUploads().Transaction_Immediate(
            CreateDesc(
                BindFlag::ShaderResource,
                0, GPUAccess::Read,
                TextureDesc::Empty(), initializer),
            pkt.get());

            //  We don't have to change the SRGB modes here -- the caller should select the
            //  right srgb mode when creating a shader resource view

        if (!result)
            Throw(::Assets::Exceptions::ConstructionError(::Assets::Exceptions::ConstructionError::Reason::Unknown, nullptr, "Failure while attempting to load texture immediately"));

        auto desc = result->GetUnderlying()->GetDesc();
        assert(desc._type == BufferDesc::Type::Texture);
        return Metal::ShaderResourceView(
            result->GetUnderlying(),
            {ResolveFormatImmediate(desc._textureDesc._format, init, splitter)});
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class CachedTextureFormats
    {
    public:
        class Desc {};

        CachedTextureFormats(const Desc&);
        ~CachedTextureFormats();

        typedef std::pair<uint64, Format> Entry;
        OSServices::MemoryMappedFile _cache;

        class Header
        {
        public:
            unsigned _count;
        };

        static const unsigned MaxCachedTextures = 10*1024;
    };

    CachedTextureFormats::CachedTextureFormats(const Desc&)
    {
        unsigned entrySize = sizeof(Entry);
            
            //  use a memory mapped file for this. This way, we never have to 
            //  worry about flushing out to disk... The OS will take care of 
            //  committing the results to disk on exit
		auto size = entrySize * MaxCachedTextures + sizeof(Header);
		auto ioResult = ::Assets::MainFileSystem::TryOpen(
			_cache,
            "int/TextureFormatCache.dat", size,
			"r+", 0u);
		if (ioResult != ::Assets::IFileSystem::IOReason::Success) {
			_cache = ::Assets::MainFileSystem::OpenMemoryMappedFile("int/TextureFormatCache.dat", size, "w", 0u);
			XlClearMemory(_cache.GetData().begin(), size);
		}
    }

    CachedTextureFormats::~CachedTextureFormats() {}

    static bool IsDXTNormalMapFormat(Format format)
    {
        return unsigned(format) >= unsigned(RenderCore::Format::BC1_TYPELESS)
            && unsigned(format) <= unsigned(RenderCore::Format::BC1_UNORM_SRGB);
    }

    bool DeferredShaderResource::IsDXTNormalMap(StringSection<::Assets::ResChar> textureName)
    {
        if (textureName.IsEmpty()) return false;

        auto& cache = ConsoleRig::FindCachedBox<CachedTextureFormats>(
            CachedTextureFormats::Desc());

        typedef CachedTextureFormats::Header Hdr;
        typedef CachedTextureFormats::Entry Entry;
        auto* data = cache._cache.GetData().begin();
        if (!data) {
            static bool firstTime = true;
            if (firstTime) {
                Log(Error) << "Failed to open TextureFormatCache.dat! DXT normal map queries will be inefficient." << std::endl;
                firstTime = false;
            }
            return IsDXTNormalMapFormat(LoadFormat(textureName));
        }

        auto& hdr = *(Hdr*)data;
        auto* start = (Entry*)PtrAdd(data, sizeof(Hdr));
        auto* end = (Entry*)PtrAdd(data, sizeof(Hdr) + sizeof(Entry) * hdr._count);

        auto hashName = Hash64(textureName);
        auto* i = std::lower_bound(start, end, hashName, CompareFirst<uint64, Format>());
        if (i == end || i->first != hashName) {
            if ((hdr._count+1) > CachedTextureFormats::MaxCachedTextures) {
                assert(0);  // cache has gotten too big
                return false;
            }

            std::move_backward(i, end, end+1);
            i->first = hashName;
            TRY {
                i->second = DeferredShaderResource::LoadFormat(textureName);
            } CATCH (const ::Assets::Exceptions::InvalidAsset&) {
                i->second = Format::Unknown;
            } CATCH_END
            ++hdr._count;
            return IsDXTNormalMapFormat(i->second);
        }

        return IsDXTNormalMapFormat(i->second);
    }


	DeferredShaderResource::DeferredShaderResource(
		const Metal::ShaderResourceView& srv,
		const std::string& initializer,
		const ::Assets::DepValPtr& depVal)
	: _srv(srv), _initializer(initializer), _depVal(depVal)
	{}

	DeferredShaderResource::~DeferredShaderResource()
    {
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	ParameterBox TechParams_SetResHas(
        const ParameterBox& inputMatParameters, const ParameterBox& resBindings,
        const ::Assets::DirectorySearchRules& searchRules)
    {
        static const auto DefaultNormalsTextureBindingHash = ParameterBox::MakeParameterNameHash("NormalsTexture");
            // The "material parameters" ParameterBox should contain some "RES_HAS_..."
            // settings. These tell the shader what resource bindings are available
            // (and what are missing). We need to set these parameters according to our
            // binding list
        ParameterBox result = inputMatParameters;
        for (const auto& param:resBindings) {
            result.SetParameter(StringMeld<64, utf8>() << "RES_HAS_" << param.Name(), 1);
            if (param.HashName() == DefaultNormalsTextureBindingHash) {
                auto resourceName = resBindings.GetParameterAsString(DefaultNormalsTextureBindingHash);
                if (resourceName.has_value()) {
                    ::Assets::ResChar resolvedName[MaxPath];
                    searchRules.ResolveFile(resolvedName, dimof(resolvedName), resourceName.value().c_str());
                    result.SetParameter(
                        (const utf8*)"RES_HAS_NormalsTexture_DXT", 
                        DeferredShaderResource::IsDXTNormalMap(resolvedName));
                }
            }
        }
        return std::move(result);
    }
}}
