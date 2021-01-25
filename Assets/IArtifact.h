// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IAsyncMarker.h"
#include "ICompileOperation.h"
#include "AssetsCore.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/MemoryUtils.h"
#include <memory>
#include <functional>

namespace Assets
{
	class IFileInterface;
	using ArtifactReopenFunction = std::function<std::shared_ptr<IFileInterface>()>;

    class ArtifactRequest
    {
    public:
		const char*		_name;		// for debugging purposes, to make it easier to track requests
        uint64_t 		_type;
        unsigned        _expectedVersion;
        
        enum class DataType
        {
            ReopenFunction, 
			Raw, BlockSerializer,
			SharedBlob
        };
        DataType        _dataType;
    };

    class ArtifactRequestResult
    {
    public:
        std::unique_ptr<uint8[], PODAlignedDeletor> _buffer;
        size_t                                      _bufferSize = 0;
		Blob										_sharedBlob;
		ArtifactReopenFunction						_reopenFunction;
    };

	class IArtifactCollection
	{
	public:
		virtual std::vector<ArtifactRequestResult> ResolveRequests(IteratorRange<const ArtifactRequest*> requests) const = 0;
		virtual Blob					GetBlob() const = 0;
		virtual DepValPtr				GetDependencyValidation() const = 0;
		virtual StringSection<ResChar>	GetRequestParameters() const = 0;		// these are parameters that should be passed through to the asset when it's actually loaded from the blob
		virtual ~IArtifactCollection();
	};

    /// <summary>Records the state of a resource being compiled</summary>
    /// When a resource compile operation begins, we need some generic way
    /// to test it's state. We also need some breadcrumbs to find the final 
    /// result when the compile is finished.
    ///
    /// This class acts as a bridge between the compile operation and
    /// the final resource class. Therefore, we can interchangeable mix
    /// and match different resource implementations and different processing
    /// solutions.
    ///
    /// Sometimes just a filename to the processed resource will be enough.
    /// Other times, objects are stored in a "ArchiveCache" object. For example,
    /// shader compiles are typically combined together into archives of a few
    /// different configurations. So a pointer to an optional ArchiveCache is provided.
    class ArtifactCollectionFuture : public GenericFuture
    {
    public:
		const std::shared_ptr<IArtifactCollection>& GetArtifactCollection();
		Blob GetErrorMessage();

        ArtifactCollectionFuture();
        ~ArtifactCollectionFuture();

		ArtifactCollectionFuture(ArtifactCollectionFuture&&) = delete;
		ArtifactCollectionFuture& operator=(ArtifactCollectionFuture&&) = delete;
		ArtifactCollectionFuture(const ArtifactCollectionFuture&) = delete;
		ArtifactCollectionFuture& operator=(const ArtifactCollectionFuture&) = delete;

		void SetArtifactCollection(
			::Assets::AssetState newState,
			const std::shared_ptr<IArtifactCollection>& artifacts);

	private:
		std::shared_ptr<IArtifactCollection> _artifactCollection;
    };

	void QueueCompileOperation(
		const std::shared_ptr<ArtifactCollectionFuture>& future,
		std::function<void(ArtifactCollectionFuture&)>&& operation);

///////////////////////////////////////////////////////////////////////////////////////////////////

	class ChunkFileArtifactCollection : public IArtifactCollection
	{
	public:
		std::vector<ArtifactRequestResult> ResolveRequests(IteratorRange<const ArtifactRequest*> requests) const;
		Blob GetBlob() const;
		DepValPtr GetDependencyValidation() const;
		StringSection<ResChar> GetRequestParameters() const;
		ChunkFileArtifactCollection(
			const std::shared_ptr<IFileInterface>& file,
			const DepValPtr& depVal,
			const std::string& requestParameters = {});
		~ChunkFileArtifactCollection();
	private:
		std::shared_ptr<IFileInterface> _file;
		DepValPtr _depVal;
		std::string _requestParameters;
	};

	class BlobArtifactCollection : public IArtifactCollection
	{
	public:
		std::vector<ArtifactRequestResult> ResolveRequests(IteratorRange<const ArtifactRequest*> requests) const;
		Blob GetBlob() const;
		DepValPtr GetDependencyValidation() const;
		StringSection<ResChar> GetRequestParameters() const;
		BlobArtifactCollection(
			IteratorRange<const ICompileOperation::SerializedArtifact*> chunks, 
			const DepValPtr& depVal, 
			const std::string& collectionName = {},
			const std::string& requestParams = {});
		~BlobArtifactCollection();
	private:
		std::vector<ICompileOperation::SerializedArtifact> _chunks;
		DepValPtr _depVal;
		std::string _collectionName;
		std::string _requestParams;
	};

	class CompilerExceptionArtifact : public ::Assets::IArtifactCollection
	{
	public:
		std::vector<ArtifactRequestResult> ResolveRequests(IteratorRange<const ArtifactRequest*> requests) const;
		Blob GetBlob() const;
		DepValPtr GetDependencyValidation() const;
		StringSection<::Assets::ResChar> GetRequestParameters() const;
		CompilerExceptionArtifact(
			const ::Assets::Blob& log,
			const ::Assets::DepValPtr& depVal);
		~CompilerExceptionArtifact();
	private:
		::Assets::Blob _log;
		::Assets::DepValPtr _depVal;
	};

}


