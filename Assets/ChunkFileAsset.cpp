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
#include "../ConsoleRig/Log.h"

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

            // First scan through and check to see if we
            // have all of the chunks we need
        using ChunkHeader = Serialization::ChunkFile::ChunkHeader;
        for (const auto& r:requests) {
            auto i = std::find_if(
                chunks.begin(), chunks.end(), 
                [&r](const ChunkHeader& c) { return c._type == r._type; });
            if (i == chunks.end())
                Throw(::Assets::Exceptions::FormatError(
                    StringMeld<128>() << "Missing chunk (" << r._name << ")", filename));

            if (i->_chunkVersion != r._expectedVersion)
                Throw(::Assets::Exceptions::FormatError(
                    ::Assets::Exceptions::FormatError::Reason::UnsupportedVersion,
                    StringMeld<256>() 
                        << "Data chunk is incorrect version for chunk (" 
                        << r._name << ") expected: " << r._expectedVersion << ", got: " << i->_chunkVersion, 
                        filename));
        }

        for (const auto& r:requests) {
            auto i = std::find_if(
                chunks.begin(), chunks.end(), 
                [&r](const ChunkHeader& c) { return c._type == r._type; });
            assert(i != chunks.end());

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
    : _assetTypeName(assetTypeName)
    {
        _pendingResolveOp._fn = nullptr;
        _completedState = ::Assets::AssetState::Pending;
    }

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

    void ChunkFileAsset::Prepare(const ResChar filename[], const ResolveOp& op)
    {
        assert(!_pendingCompile && !_pendingResolveOp._fn && _completedState == ::Assets::AssetState::Pending);
        _filename = filename;

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        RegisterFileDependency(_validationCallback, filename);
        auto pendingResult = LoadRawData(filename, op._requests);
        ExecuteResolve(op._fn, this, MakeIteratorRange(pendingResult), filename, _assetTypeName);
        _completedState = ::Assets::AssetState::Ready;
    }
    
    void ChunkFileAsset::Prepare(::Assets::ICompileMarker& marker, const ResolveOp& op)
    {
        assert(!_pendingCompile && !_pendingResolveOp._fn && _completedState == ::Assets::AssetState::Pending);

        // If we have an existing marker, we will try to load it here.
        // If the load fails, we can either mark the asset as invalid; or
        // we can invoke a recompile (which will happen asynchronously).
        // We are accessing the disk here, so ideally Prepare should be called
        // from a background thread.
        auto existing = marker.GetExistingAsset();
        if (existing._dependencyValidation && existing._dependencyValidation->GetValidationIndex() == 0) {
            TRY
            {
                auto pendingResult = LoadRawData(existing._sourceID0, op._requests);
                (*op._fn)(this, MakeIteratorRange(pendingResult));
                _filename = existing._sourceID0;
                _validationCallback = existing._dependencyValidation;
                _completedState = ::Assets::AssetState::Ready;
                return;
            }

            // We should catch only some exceptions and force a recompile... This should happen on
            // missing file, or if the file has a bad version number. If the load fails for other
            // reasons, we just throw back the exception.
            CATCH(const ::Assets::Exceptions::FormatError& e) 
            {
                if (e.GetReason() != ::Assets::Exceptions::FormatError::Reason::UnsupportedVersion)
                    throw;

                LogWarning << "Asset (" << existing._sourceID0 << ") appears to be incorrect version. Attempting recompile.";
            }
            CATCH(const Utility::Exceptions::IOException& e)
            {
                if (e.GetReason() != Utility::Exceptions::IOException::Reason::FileNotFound)
                    throw;

                LogWarning << "Asset (" << existing._sourceID0 << ") is missing. Attempting compile.";
            }
            CATCH_END
        }

        _pendingResolveOp = op;
        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        _pendingCompile = marker.InvokeCompile();
    }

    ChunkFileAsset::ChunkFileAsset(ChunkFileAsset&& moveFrom) never_throws
    : _filename(std::move(moveFrom._filename))
    , _pendingResolveOp(moveFrom._pendingResolveOp)
    , _pendingCompile(moveFrom._pendingCompile)
    , _validationCallback(std::move(moveFrom._validationCallback))
    , _assetTypeName(moveFrom._assetTypeName)
    , _completedState(moveFrom._completedState)
    {}

    ChunkFileAsset& ChunkFileAsset::operator=(ChunkFileAsset&& moveFrom) never_throws
    {
        _filename = std::move(moveFrom._filename);
        _pendingResolveOp = moveFrom._pendingResolveOp;
        _pendingCompile = moveFrom._pendingCompile;
        _validationCallback = std::move(moveFrom._validationCallback);
        _assetTypeName = moveFrom._assetTypeName;
        _completedState = moveFrom._completedState;
        return *this;
    }

    ChunkFileAsset::~ChunkFileAsset() {}

    void ChunkFileAsset::Resolve() const
    {
        if (_pendingCompile) {
            auto markerState = _pendingCompile->GetAssetState();
            if (markerState == ::Assets::AssetState::Pending) {
                    // we need to throw immediately on pending resource
                    // this object is useless while it's pending.
                Throw(::Assets::Exceptions::PendingAsset(
                    _pendingCompile->Initializer(), 
                    StringMeld<256>() << "Compile still pending (type:" << _assetTypeName << ")"));
            }

                // hack --  Resolve needs to be called by const methods (like "GetStaticBoundingBox")
                //          but Resolve() must change all the internal pointers... It's an awkward
                //          case for const-correctness
            const_cast<ChunkFileAsset*>(this)->CompleteFromMarker(*_pendingCompile);
            _pendingCompile.reset();
        }

        if (_completedState == ::Assets::AssetState::Invalid)
            Throw(::Assets::Exceptions::InvalidAsset(
                _filename.c_str(), 
                StringMeld<256>() << "Pending compile failed in ChunkFileAsset (type: " << _assetTypeName << ")"));
    }

    ::Assets::AssetState ChunkFileAsset::TryResolve() const
    {
        if (_pendingCompile) {
            auto markerState = _pendingCompile->GetAssetState();
            if (markerState == ::Assets::AssetState::Pending) return markerState;
            const_cast<ChunkFileAsset*>(this)->CompleteFromMarker(*_pendingCompile);
            _pendingCompile.reset();
        }

        return _completedState;
    }

    ::Assets::AssetState ChunkFileAsset::StallAndResolve() const
    {
        if (_pendingCompile) {
            auto markerState = _pendingCompile->StallWhilePending();
            if (markerState == ::Assets::AssetState::Pending) return markerState;
            const_cast<ChunkFileAsset*>(this)->CompleteFromMarker(*_pendingCompile);
            _pendingCompile.reset();
        }

        return _completedState;
    }

    void ChunkFileAsset::CompleteFromMarker(::Assets::PendingCompileMarker& marker)
    {
        assert(_completedState == Assets::AssetState::Pending);
        _completedState = marker.GetAssetState();
        assert(_completedState != Assets::AssetState::Pending);

        auto locator = marker.GetLocator();

        if (locator._dependencyValidation) {
            if (!_validationCallback) { _validationCallback = locator._dependencyValidation; }
            else { ::Assets::RegisterAssetDependency(_validationCallback, locator._dependencyValidation); }
        }
        
        _filename = locator._sourceID0;

        if (_completedState == Assets::AssetState::Ready) {
            auto chunks = LoadRawData(locator._sourceID0, _pendingResolveOp._requests);
            if (_pendingResolveOp._fn) {
                ExecuteResolve(_pendingResolveOp._fn, this, MakeIteratorRange(chunks), _filename.c_str(), _assetTypeName);
                _pendingResolveOp._fn = nullptr;
            }
        }
    }

}

