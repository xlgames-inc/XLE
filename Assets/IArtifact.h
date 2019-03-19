// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "GenericFuture.h"
#include "AssetsCore.h"
#include "../Utility/IteratorUtils.h"
#include <memory>

namespace Assets
{
	class IArtifact
	{
	public:
		virtual Blob					GetBlob() const = 0;
		virtual DepValPtr				GetDependencyValidation() const = 0;
		virtual StringSection<ResChar>	GetRequestParameters() const = 0;		// these are parameters that should be passed through to the asset when it's actually loaded from the blob
		virtual ~IArtifact();
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
    class ArtifactFuture : public GenericFuture
    {
    public:
		using NameAndArtifact = std::pair<std::string, std::shared_ptr<IArtifact>>;
		IteratorRange<const NameAndArtifact*> GetArtifacts() const { return MakeIteratorRange(_artifacts); }

        ArtifactFuture();
        ~ArtifactFuture();

		ArtifactFuture(ArtifactFuture&&) = delete;
		ArtifactFuture& operator=(ArtifactFuture&&) = delete;
		ArtifactFuture(const ArtifactFuture&) = delete;
		ArtifactFuture& operator=(const ArtifactFuture&) = delete;

		void AddArtifact(const std::string& name, const std::shared_ptr<IArtifact>& artifact);

	private:
		std::vector<NameAndArtifact> _artifacts;
    };

	/// <summary>Returned from a IAssetCompiler on response to a compile request</summary>
	/// After receiving a compile marker, the caller can choose to either retrieve an existing
	/// artifact from a previous compile, or begin a new asynchronous compile operation.
	class IArtifactCompileMarker
	{
	public:
		virtual std::shared_ptr<IArtifact> GetExistingAsset() const = 0;
		virtual std::shared_ptr<ArtifactFuture> InvokeCompile() const = 0;
		virtual StringSection<ResChar> Initializer() const = 0;
		virtual ~IArtifactCompileMarker();
	};

///////////////////////////////////////////////////////////////////////////////////////////////////

	class FileArtifact : public IArtifact
	{
	public:
		Blob		GetBlob() const;
		DepValPtr	GetDependencyValidation() const;
		StringSection<ResChar>	GetRequestParameters() const;
		FileArtifact(const rstring& filename, const DepValPtr& depVal);
		~FileArtifact();
	private:
		rstring _filename;
		DepValPtr _depVal;
		rstring _params;
	};

	class BlobArtifact : public IArtifact
	{
	public:
		Blob		GetBlob() const;
		DepValPtr	GetDependencyValidation() const;
		StringSection<ResChar>	GetRequestParameters() const;
		BlobArtifact(const Blob& blob, const DepValPtr& depVal, const rstring& requestParams = {});
		~BlobArtifact();
	private:
		Blob _blob;
		DepValPtr _depVal;
		rstring _requestParams;
	};

	class CompilerExceptionArtifact : public ::Assets::IArtifact
	{
	public:
		Blob		GetBlob() const;
		DepValPtr	GetDependencyValidation() const;
		StringSection<::Assets::ResChar>	GetRequestParameters() const;
		CompilerExceptionArtifact(const ::Assets::Blob& log, const ::Assets::DepValPtr& depVal);
		~CompilerExceptionArtifact();
	private:
		::Assets::Blob _log;
		::Assets::DepValPtr _depVal;
	};

}


