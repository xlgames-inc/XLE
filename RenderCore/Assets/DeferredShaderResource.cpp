// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeferredShaderResource.h"
#include "../Metal/ShaderResource.h"
#include "../../Assets/AsyncLoadOperation.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../BufferUploads/ResourceLocator.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Threading/CompletionThreadPool.h"
#include "../../Foreign/tinyxml2-master/tinyxml2.h"

#include "../../Core/WinAPI/IncludeWindows.h"

namespace RenderCore { namespace Assets 
{
    using ResChar = ::Assets::ResChar;

    static BufferUploads::IManager* s_bufferUploads = nullptr;
    void SetBufferUploads(BufferUploads::IManager* bufferUploads)
    {
        s_bufferUploads = bufferUploads;
    }

    enum class SourceColorSpace { SRGB, Linear, Unspecified };

    static CompletionThreadPool& GetUtilityThreadPool()
    {
            // todo -- should go into reusable location
        static CompletionThreadPool s_threadPool(4);
        return s_threadPool;
    }

    class MetadataLoadMarker : public ::Assets::AsyncLoadOperation
    {
    public:
        SourceColorSpace _colorSpace;

        virtual ::Assets::AssetState Complete(const void* buffer, size_t bufferSize);
        MetadataLoadMarker() : _colorSpace(SourceColorSpace::Unspecified) {}
    };

    bool LoadColorSpaceFromMetadataFile(SourceColorSpace& result, const void* start, size_t size)
    {
        result = SourceColorSpace::Unspecified;

        if (!start || !size) return false;

        // skip over the "byte order mark", if it exists...
        if (size >= 3 && ((const uint8*)start)[0] == 0xef && ((const uint8*)start)[1] == 0xbb && ((const uint8*)start)[2] == 0xbf) {
            start = PtrAdd(start, 3);
            size -= 3;
        }

        using namespace tinyxml2;
        XMLDocument doc;
	    auto e = doc.Parse((const char*)start, size);
        if (e != XML_SUCCESS) return false;

        const auto* root = doc.RootElement();
        if (root) {
            auto colorSpace = root->FindAttribute("colorSpace");
            if (colorSpace) {
                if (!XlCompareStringI(colorSpace->Value(), "srgb")) { result = SourceColorSpace::SRGB; }
                else if (!XlCompareStringI(colorSpace->Value(), "linear")) { result = SourceColorSpace::Linear; }
            }
        }

        return true;
    }

    ::Assets::AssetState MetadataLoadMarker::Complete(const void* buffer, size_t bufferSize)
    {
            // Attempt to parse the xml in our data buffer...
        if (!LoadColorSpaceFromMetadataFile(_colorSpace, buffer, bufferSize))
            return ::Assets::AssetState::Invalid;

        return ::Assets::AssetState::Ready;
    }

    class DeferredShaderResource::Pimpl
    {
    public:
        BufferUploads::TransactionID _transaction;
        intrusive_ptr<BufferUploads::ResourceLocator> _locator;
        Metal::ShaderResourceView _srv;

        SourceColorSpace _colSpaceRequestString;
        SourceColorSpace _colSpaceDefault;
        std::shared_ptr<MetadataLoadMarker> _metadataMarker;
    };

    class DecodedInitializer
    {
    public:
        const ResChar*      _filenameStart;
        const ResChar*      _filenameEnd;

        SourceColorSpace    _colSpaceRequestString;
        SourceColorSpace    _colSpaceDefault;
        bool                _generateMipmaps;

        DecodedInitializer(const ResChar initializer[]);
    };

    DecodedInitializer::DecodedInitializer(const ResChar initializer[])
    {
        _generateMipmaps = true;
        _colSpaceRequestString = SourceColorSpace::Unspecified;
        _colSpaceDefault = SourceColorSpace::Unspecified;

        auto* colon = XlFindCharReverse(initializer, ':');
        if (colon) {
            for (auto* c = colon + 1; *c; ++c) {
                if (*c == 'l' || *c == 'L') { _colSpaceRequestString = SourceColorSpace::Linear; }
                if (*c == 's' || *c == 'S') { _colSpaceRequestString = SourceColorSpace::SRGB; }
                if (*c == 't' || *c == 'T') { _generateMipmaps = false; }
            }
        } else
            colon = &initializer[XlStringLen(initializer)];

        if (_colSpaceRequestString == SourceColorSpace::Unspecified) {
            if (XlFindStringI(initializer, "_ddn")) {
                _colSpaceDefault = SourceColorSpace::Linear;
            } else {
                _colSpaceDefault = SourceColorSpace::SRGB;
            }
        }

        _filenameStart = initializer;
        _filenameEnd = colon;
    }

    DeferredShaderResource::DeferredShaderResource(const ResChar initializer[])
    {
        DEBUG_ONLY(XlCopyString(_initializer, dimof(_initializer), initializer);)
        _pimpl = std::make_unique<Pimpl>();

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();

            // parse initialiser for flags
        DecodedInitializer init(initializer);
        _pimpl->_colSpaceRequestString = init._colSpaceRequestString;
        _pimpl->_colSpaceDefault = init._colSpaceDefault;

        if (_pimpl->_colSpaceRequestString == SourceColorSpace::Unspecified) {
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
            
            ::Assets::ResChar filename[MaxPath];
            XlCopyNString(filename, dimof(filename), 
                init._filenameStart, init._filenameEnd - init._filenameStart);
            XlCatString(filename, dimof(filename), ".metadata");
            RegisterFileDependency(_validationCallback, filename);

            _pimpl->_metadataMarker = std::make_shared<MetadataLoadMarker>(filename);
            _pimpl->_metadataMarker->Enqueue(GetUtilityThreadPool());
        }

        using namespace BufferUploads;
        TextureLoadFlags::BitField flags = init._generateMipmaps ? TextureLoadFlags::GenerateMipmaps : 0;
        auto pkt = CreateStreamingTextureSource(init._filenameStart, init._filenameEnd, flags);
        _pimpl->_transaction = s_bufferUploads->Transaction_Begin(
            CreateDesc(
                BindFlag::ShaderResource,
                0, GPUAccess::Read,
                TextureDesc::Empty(), initializer),
            pkt.get());

        RegisterFileDependency(_validationCallback, initializer);
    }

    DeferredShaderResource::~DeferredShaderResource()
    {
        if (_pimpl->_transaction != ~BufferUploads::TransactionID(0))
            s_bufferUploads->Transaction_End(_pimpl->_transaction);
    }

    const Metal::ShaderResourceView&       DeferredShaderResource::GetShaderResource() const
    {
        if (!_pimpl->_srv.GetUnderlying()) {
            if (_pimpl->_transaction == ~BufferUploads::TransactionID(0))
                ThrowException(::Assets::Exceptions::InvalidResource(Initializer(), "Unknown error during loading"));

            if (!s_bufferUploads->IsCompleted(_pimpl->_transaction))
                ThrowException(::Assets::Exceptions::PendingResource(Initializer(), ""));

            _pimpl->_locator = s_bufferUploads->GetResource(_pimpl->_transaction);
            s_bufferUploads->Transaction_End(_pimpl->_transaction);
            _pimpl->_transaction = ~BufferUploads::TransactionID(0);

            if (!_pimpl->_locator || !_pimpl->_locator->GetUnderlying())
                ThrowException(::Assets::Exceptions::InvalidResource(Initializer(), "Unknown error during loading"));

            auto desc = BufferUploads::ExtractDesc(*_pimpl->_locator->GetUnderlying());
            if (desc._type != BufferUploads::BufferDesc::Type::Texture)
                ThrowException(::Assets::Exceptions::InvalidResource(Initializer(), "Unknown error during loading"));

                // calculate the color space to use (resolving the defaults, request string and metadata)
            auto colSpace = SourceColorSpace::SRGB;
            if (_pimpl->_colSpaceRequestString != SourceColorSpace::Unspecified) colSpace = _pimpl->_colSpaceRequestString;
            else {
                if (_pimpl->_colSpaceDefault != SourceColorSpace::Unspecified) colSpace = _pimpl->_colSpaceDefault;

                if (_pimpl->_metadataMarker) {
                    auto state = _pimpl->_metadataMarker->GetState();
                    if (state == ::Assets::AssetState::Pending)
                        ThrowException(::Assets::Exceptions::PendingResource(Initializer(), ""));

                    if (state == ::Assets::AssetState::Ready && _pimpl->_metadataMarker->_colorSpace != SourceColorSpace::Unspecified) {
                        colSpace = _pimpl->_metadataMarker->_colorSpace;
                    }
                }
            }

            auto format = (Metal::NativeFormat::Enum)desc._textureDesc._nativePixelFormat;
            if (colSpace == SourceColorSpace::SRGB) format = Metal::AsSRGBFormat(format);
            else if (colSpace == SourceColorSpace::Linear) format = Metal::AsLinearFormat(format);

            _pimpl->_srv = Metal::ShaderResourceView(_pimpl->_locator->GetUnderlying(), format);
        }

        return _pimpl->_srv;
    }

    const char*                     DeferredShaderResource::Initializer() const
    {
        #if defined(_DEBUG)
            return _initializer;
        #else
            return "";
        #endif
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static Metal::NativeFormat::Enum ResolveFormatImmediate(Metal::NativeFormat::Enum typelessFormat, const DecodedInitializer& init)
    {
        auto result = typelessFormat;
        if (Metal::HasLinearAndSRGBFormats(result)) {
            auto finalColSpace = init._colSpaceRequestString;
            if (finalColSpace == SourceColorSpace::Unspecified) {
                    // need to load the metadata file to get SRGB settings!
                ::Assets::ResChar metadataFile[MaxPath];
                XlCopyNString(metadataFile, dimof(metadataFile), init._filenameStart, init._filenameEnd - init._filenameStart);
                XlCatString(metadataFile, dimof(metadataFile), ".metadata");

                size_t filesize = 0;
                auto rawFile = LoadFileAsMemoryBlock(metadataFile, &filesize);
                if (rawFile.get())
                    LoadColorSpaceFromMetadataFile(finalColSpace, rawFile.get(), filesize);
            
                if (finalColSpace == SourceColorSpace::Unspecified)
                    finalColSpace = (init._colSpaceDefault != SourceColorSpace::Unspecified) ? init._colSpaceDefault : SourceColorSpace::SRGB;
            }

            if (finalColSpace == SourceColorSpace::SRGB) result = Metal::AsSRGBFormat(result);
            else if (finalColSpace == SourceColorSpace::Linear) result = Metal::AsLinearFormat(result);
        }
        return result;
    }

    Metal::NativeFormat::Enum DeferredShaderResource::LoadFormat(const ::Assets::ResChar initializer[])
    {
        DecodedInitializer init(initializer);
        auto result = (Metal::NativeFormat::Enum)BufferUploads::LoadTextureFormat(init._filenameStart, init._filenameEnd)._nativePixelFormat;
        return ResolveFormatImmediate(result, init);
    }

    Metal::ShaderResourceView DeferredShaderResource::LoadImmediately(const char initializer[])
    {
        DecodedInitializer init(initializer);

        using namespace BufferUploads;
        TextureLoadFlags::BitField flags = init._generateMipmaps ? TextureLoadFlags::GenerateMipmaps : 0;
        auto pkt = CreateStreamingTextureSource(init._filenameStart, init._filenameEnd, flags);
        auto result = s_bufferUploads->Transaction_Immediate(
            CreateDesc(
                BindFlag::ShaderResource,
                0, GPUAccess::Read,
                TextureDesc::Empty(), initializer),
            pkt.get());

            //  We don't have to change the SRGB modes here -- the caller should select the
            //  right srgb mode when creating a shader resource view

        if (!result)
            ThrowException(::Assets::Exceptions::InvalidResource(initializer, "Failure while attempting to load texture immediately"));

        auto desc = ExtractDesc(*result->GetUnderlying());
        assert(desc._type == BufferDesc::Type::Texture);
        return Metal::ShaderResourceView(
            result->GetUnderlying(), 
            ResolveFormatImmediate((Metal::NativeFormat::Enum)desc._textureDesc._nativePixelFormat, init));
    }

}}

