// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ChunkFileAsset.h"
#include "BlockSerializer.h"
#include "IntermediateAssets.h"
#include "../Utility/StringFormat.h"
#include "../Core/Exceptions.h"

namespace Assets
{
    static std::vector<AssetChunkResult> LoadRawData(
        const char filename[],
        IteratorRange<const AssetChunkRequest*> requests)
    {
        BasicFile file(filename, "rb");
        auto chunks = Serialization::ChunkFile::LoadChunkTable(file);

        std::vector<AssetChunkResult> result;
        result.reserve(requests.size());

        using ChunkHeader = Serialization::ChunkFile::ChunkHeader;
        for (const auto& r:requests) {
            auto i = std::find_if(chunks.begin(), chunks.end(), 
                [&r](const ChunkHeader& c) { return c._type == r._type; });
            if (i == chunks.end())
                throw ::Assets::Exceptions::FormatError(
                    StringMeld<128>() << "Missing chunk (" << r._name << ")", filename);

            if (i->_chunkVersion != r._expectedVersion) {
                throw ::Assets::Exceptions::FormatError(
                    StringMeld<256>() << 
                        "Data chunk is incorrect version for chunk (" 
                        << r._name << ") expected: " << r._expectedVersion << ", got: " << i->_chunkVersion, 
                    filename);
            }

            AssetChunkResult chunkResult;
            chunkResult._offset = i->_fileOffset;
            chunkResult._size = i->_size;

            if (r._dataType != AssetChunkRequest::DataType::DontLoad) {
                chunkResult._buffer = std::make_unique<uint8[]>(i->_size);
                file.Seek(i->_fileOffset, SEEK_SET);
                file.Read(chunkResult._buffer.get(), 1, i->_size);

                // initialize with the block serializer (if requested)
                if (r._dataType == AssetChunkRequest::DataType::BlockSerializer)
                    Serialization::Block_Initialize(chunkResult._buffer.get());
            }

            result.emplace_back(std::move(chunkResult));
        }

        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    ChunkFileAsset::ChunkFileAsset(const char assetTypeName[])
    : _resolveFn(nullptr), _assetTypeName(assetTypeName)
    {}

    void ChunkFileAsset::ExecuteResolve(
        ResolveFn* resolveFn, void* obj, 
        IteratorRange<AssetChunkResult*> chunks, const ResChar filename[],
        const char assetNameType[])
    {
            // Just run the resolve function, and convert any exceptions into
            // InvalidAsset type exceptions
        if (resolveFn) {
            TRY { (*resolveFn)(obj, chunks); } 
            CATCH(const Exceptions::InvalidAsset&) { throw; }
            CATCH(const std::exception& e) {
                throw Exceptions::InvalidAsset(filename, StringMeld<1024>() << "Exception during chunk file resolve (asset type: " << assetNameType << ") Exception:" << e.what());
            } CATCH(...) {
                throw Exceptions::InvalidAsset(filename, StringMeld<1024>() << "Unknown exception during chunk file resolve (asset type: " << assetNameType << ")");
            } CATCH_END
        }
    }

    void ChunkFileAsset::Prepare(
        const ResChar filename[], 
        IteratorRange<const AssetChunkRequest*> requests, 
        ResolveFn* resolveFn)
    {
        assert(!_validationCallback && !_resolveFn);
        _filename = filename;

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        RegisterFileDependency(_validationCallback, filename);
        auto pendingResult = LoadRawData(filename, requests);
        ExecuteResolve(resolveFn, this, MakeIteratorRange(pendingResult), filename, _assetTypeName);
    }
    
    void ChunkFileAsset::Prepare(
        std::shared_ptr<::Assets::PendingCompileMarker>&& marker,
        IteratorRange<const AssetChunkRequest*> requests,
        ResolveFn* resolveFn)
    {
        assert(!_validationCallback && !_resolveFn);
        if (marker->GetState() == ::Assets::AssetState::Ready) {
            _resolveFn = nullptr;
            _filename = marker->_sourceID0;
            _validationCallback = marker->_dependencyValidation;
            auto pendingResult = LoadRawData(marker->_sourceID0, requests);
            (*resolveFn)(this, MakeIteratorRange(pendingResult));
        } else {
            _marker = std::move(marker);
            _validationCallback = std::make_shared<::Assets::DependencyValidation>();
            _requests = requests;
            _resolveFn = resolveFn;
        }
    }

    ChunkFileAsset::ChunkFileAsset(ChunkFileAsset&& moveFrom) never_throws
    : _filename(std::move(moveFrom._filename))
    , _requests(moveFrom._requests)
    , _resolveFn(std::move(moveFrom._resolveFn))
    , _marker(std::move(moveFrom._marker))
    , _validationCallback(std::move(moveFrom._validationCallback))
    , _assetTypeName(moveFrom._assetTypeName)
    {}

    ChunkFileAsset& ChunkFileAsset::operator=(ChunkFileAsset&& moveFrom) never_throws
    {
        _filename = std::move(moveFrom._filename);
        _requests = moveFrom._requests;
        _resolveFn = std::move(moveFrom._resolveFn);
        _marker = std::move(moveFrom._marker);
        _validationCallback = std::move(moveFrom._validationCallback);
        _assetTypeName = moveFrom._assetTypeName;
        return *this;
    }

    ChunkFileAsset::~ChunkFileAsset() {}

    void ChunkFileAsset::Resolve() const
    {
        if (_marker) {
            if (_marker->GetState() == ::Assets::AssetState::Invalid) {
                Throw(::Assets::Exceptions::InvalidAsset(
                    _marker->Initializer(), 
                    StringMeld<256>() << "Pending compile failed in ChunkFileAsset (type: " << _assetTypeName << ")"));
            } else if (_marker->GetState() == ::Assets::AssetState::Pending) {
                    // we need to throw immediately on pending resource
                    // this object is useless while it's pending.
                Throw(::Assets::Exceptions::PendingAsset(
                    _marker->Initializer(), 
                    StringMeld<256>() << "Compile still pending (type:" << _assetTypeName << ")"));
            }

                // hack --  Resolve needs to be called by const methods (like "GetStaticBoundingBox")
                //          but Resolve() must change all the internal pointers... It's an awkward
                //          case for const-correctness
            const_cast<ChunkFileAsset*>(this)->CompleteFromMarker(*_marker);
            _marker.reset();
        }
    }

    ::Assets::AssetState ChunkFileAsset::TryResolve()
    {
        if (_marker) {
            auto markerState = _marker->GetState();
            if (markerState != ::Assets::AssetState::Ready) return markerState;
            CompleteFromMarker(*_marker);
            _marker.reset();
        }

        return ::Assets::AssetState::Ready;
    }

    ::Assets::AssetState ChunkFileAsset::StallAndResolve()
    {
        if (_marker) {
            auto markerState = _marker->StallWhilePending();
            if (markerState != ::Assets::AssetState::Ready) return markerState;
            CompleteFromMarker(*_marker);
            _marker.reset();
        }

        return ::Assets::AssetState::Ready;
    }

    void ChunkFileAsset::CompleteFromMarker(::Assets::PendingCompileMarker& marker)
    {
        _filename = marker._sourceID0;
        if (!_validationCallback) _validationCallback = marker._dependencyValidation;
        else ::Assets::RegisterAssetDependency(_validationCallback, marker._dependencyValidation);

        auto chunks = LoadRawData(marker._sourceID0, _requests);
        ExecuteResolve(_resolveFn, this, MakeIteratorRange(chunks), _filename.c_str(), _assetTypeName);
        _resolveFn = nullptr;
    }

}

