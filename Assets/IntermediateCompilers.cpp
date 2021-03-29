// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "IntermediateCompilers.h"
#include "AssetsCore.h"
#include "ICompileOperation.h"
#include "DepVal.h"
#include "AssetUtils.h"
#include "IntermediatesStore.h"
#include "IArtifact.h"
#include "InitializerPack.h"
#include "CompilerLibrary.h"
#include "ArchiveCache.h"
#include "../ConsoleRig/AttachableLibrary.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../OSServices/Log.h"
#include "../OSServices/RawFS.h"		// for OSServices::GetProcessPath()
#include "../Utility/Threading/Mutex.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/StringFormat.h"
#include <regex>
#include <set>
#include <unordered_map>

namespace Assets 
{
	struct ExtensionAndDelegate
	{
		std::string _name;
		ConsoleRig::LibVersionDesc _srcVersion;
		IntermediateCompilers::CompileOperationDelegate _delegate;
		IntermediateCompilers::ArchiveNameDelegate _archiveNameDelegate;
		DepValPtr _compilerLibraryDepVal;
		IntermediatesStore::CompileProductsGroupId _storeGroupId = 0;
	};

	struct DelegateAssociation
	{
		std::vector<uint64_t> _assetTypes;
		std::regex _regexFilter;
	};

///////////////////////////////////////////////////////////////////////////////////////////////////

	static DepValPtr MakeDepVal(
		IteratorRange<const DependentFileState*> deps,
		const DepValPtr& compilerDepVal)
	{
		DepValPtr depVal = AsDepVal(deps);
		if (compilerDepVal)
			RegisterAssetDependency(depVal, compilerDepVal);
		return depVal;
	}

    class IntermediateCompilers::Marker : public IIntermediateCompileMarker
    {
    public:
		using IdentifiersList = IteratorRange<const StringSection<>*>;
        std::shared_ptr<IArtifactCollection> GetExistingAsset() const;
        std::shared_ptr<ArtifactCollectionFuture> InvokeCompile();

        Marker(
            InitializerPack&& requestName,
            std::shared_ptr<ExtensionAndDelegate> delegate,
			std::shared_ptr<IntermediatesStore> intermediateStore);
        ~Marker();
    private:
		std::weak_ptr<ArtifactCollectionFuture> _activeFuture;
        std::weak_ptr<ExtensionAndDelegate> _delegate;
		std::shared_ptr<IntermediatesStore> _intermediateStore;
        InitializerPack _initializers;

		static void PerformCompile(
			const ExtensionAndDelegate& delegate,
			const InitializerPack& initializers,
			ArtifactCollectionFuture& compileMarker,
			IntermediatesStore* destinationStore);
    };

    std::shared_ptr<IArtifactCollection> IntermediateCompilers::Marker::GetExistingAsset() const
    {
        if (!_intermediateStore) return nullptr;

		auto d = _delegate.lock();
		if (!d)
			return nullptr;

		if (d->_archiveNameDelegate) {
			auto archiveEntry = d->_archiveNameDelegate(_initializers);
			if (!archiveEntry._archive.empty())
				return _intermediateStore->RetrieveCompileProducts(archiveEntry._archive, archiveEntry._entryId, d->_storeGroupId);
		}

		return _intermediateStore->RetrieveCompileProducts(_initializers.ArchivableName(), d->_storeGroupId);
    }

	void IntermediateCompilers::Marker::PerformCompile(
		const ExtensionAndDelegate& delegate,
		const InitializerPack& initializers, 
		ArtifactCollectionFuture& compileMarker,
		IntermediatesStore* destinationStore)
    {
		std::vector<DependentFileState> deps;
		assert(!initializers.IsEmpty());

		auto firstInitializer = initializers.GetInitializer<std::string>(0);		// first initializer is assumed to be a string

        TRY
        {
            auto model = delegate._delegate(initializers);
			if (!model)
				Throw(std::runtime_error("Compiler library returned null to compile request on " + initializers.ArchivableName()));

			deps = model->GetDependencies();

			std::vector<ICompileOperation::SerializedArtifact> artifactsForStore;

			auto targets = model->GetTargets();
			for (unsigned t=0; t<targets.size(); ++t) {
				const auto& target = targets[t];
				auto chunks = model->SerializeTarget(t);
				artifactsForStore.reserve(artifactsForStore.size() + chunks.size());
				for (auto&c:chunks)
					artifactsForStore.push_back(std::move(c));
			}

			auto resultantArtifacts = std::make_shared<BlobArtifactCollection>(
				MakeIteratorRange(artifactsForStore), 
				::Assets::MakeDepVal(MakeIteratorRange(deps), delegate._compilerLibraryDepVal));

			// Write out the intermediate file that lists the products of this compile operation
			if (destinationStore && !artifactsForStore.empty()) {
				bool storedInArchive = false;
				if (delegate._archiveNameDelegate) {
					auto archiveEntry = delegate._archiveNameDelegate(initializers);
					if (!archiveEntry._archive.empty()) {
						destinationStore->StoreCompileProducts(
							archiveEntry._archive,
							archiveEntry._entryId,
							archiveEntry._descriptiveName,
							delegate._storeGroupId,
							MakeIteratorRange(artifactsForStore),
							::Assets::AssetState::Ready,
							MakeIteratorRange(deps));
						storedInArchive = true;
					}
				}

				if (!storedInArchive) {
					destinationStore->StoreCompileProducts(
						initializers.ArchivableName(),
						delegate._storeGroupId,
						MakeIteratorRange(artifactsForStore),
						::Assets::AssetState::Ready,
						MakeIteratorRange(deps));
				}
			}

			if (!resultantArtifacts)
				Throw(::Exceptions::BasicLabel("Could not find target of the requested type in compile operation for (%s)", firstInitializer.c_str()));
        
			compileMarker.SetArtifactCollection(resultantArtifacts);

        } CATCH(const Exceptions::ConstructionError& e) {
			auto depVal = MakeDepVal(MakeIteratorRange(deps), delegate._compilerLibraryDepVal);
			if (deps.empty())
				RegisterFileDependency(depVal, MakeFileNameSplitter(firstInitializer).AllExceptParameters());		// fallback case -- compiler might have failed because of bad input file. Interpret the initializer as a filename and create a dep val for it
			Throw(Exceptions::ConstructionError(e, depVal));
		} CATCH(const std::exception& e) {
			auto depVal = MakeDepVal(MakeIteratorRange(deps), delegate._compilerLibraryDepVal);
			if (deps.empty())
				RegisterFileDependency(depVal, MakeFileNameSplitter(firstInitializer).AllExceptParameters());
			Throw(Exceptions::ConstructionError(e, depVal));
		} CATCH(...) {
			auto depVal = MakeDepVal(MakeIteratorRange(deps), delegate._compilerLibraryDepVal);
			if (deps.empty())
				RegisterFileDependency(depVal, MakeFileNameSplitter(firstInitializer).AllExceptParameters());
			Throw(Exceptions::ConstructionError(Exceptions::ConstructionError::Reason::Unknown, depVal, "%s", "unknown exception"));
		} CATCH_END
    }

	class IntermediateCompilers::Pimpl
    {
    public:
		Threading::Mutex _delegatesLock;
		std::vector<std::pair<RegisteredCompilerId, std::shared_ptr<ExtensionAndDelegate>>> _delegates;
		std::unordered_multimap<RegisteredCompilerId, std::string> _extensionsAndDelegatesMap;
		std::unordered_multimap<RegisteredCompilerId, DelegateAssociation> _requestAssociations;
		std::unordered_map<uint64_t, std::shared_ptr<Marker>> _markers;
		std::shared_ptr<IntermediatesStore> _store;
		RegisteredCompilerId _nextCompilerId = 1;

		Pimpl() {}
		Pimpl(const Pimpl&) = delete;
		Pimpl& operator=(const Pimpl&) = delete;
    };

    std::shared_ptr<ArtifactCollectionFuture> IntermediateCompilers::Marker::InvokeCompile()
    {
		auto activeFuture = _activeFuture.lock();
		if (activeFuture)
			return activeFuture;

        auto backgroundOp = std::make_shared<ArtifactCollectionFuture>();
        backgroundOp->SetDebugLabel(_initializers.ArchivableName());

		const bool allowBackgroundOps = true;
		if (allowBackgroundOps) {
			// Unfortunately we have to copy _initializers here, because we 
			// must allow for this marker to be reused (and both InvokeCompile 
			// and GetExistingAsset use _initializers)
			std::weak_ptr<ExtensionAndDelegate> weakDelegate = _delegate;
			std::shared_ptr<IntermediatesStore> store = _intermediateStore;
			QueueCompileOperation(
				backgroundOp,
				[weakDelegate, store, inits=_initializers](ArtifactCollectionFuture& op) {
				auto d = weakDelegate.lock();
				if (!d) {
					op.SetState(AssetState::Invalid);
					return;
				}

				PerformCompile(*d, inits, op, store.get());
			});
		} else {
			auto d = _delegate.lock();
			if (!d) {
				backgroundOp->SetState(AssetState::Invalid);
			} else {
				PerformCompile(*d, _initializers, *backgroundOp, _intermediateStore.get());
			}
		}
        
		_activeFuture = backgroundOp;
        return std::move(backgroundOp);
    }

	IntermediateCompilers::Marker::Marker(
        InitializerPack&& initializers,
        std::shared_ptr<ExtensionAndDelegate> delegate,
		std::shared_ptr<IntermediatesStore> intermediateStore)
    : _delegate(std::move(delegate)), _intermediateStore(std::move(intermediateStore))
	, _initializers(std::move(initializers))
    {
	}

	IntermediateCompilers::Marker::~Marker() {}

    std::shared_ptr<IIntermediateCompileMarker> IntermediateCompilers::Prepare(
        uint64_t typeCode, 
        InitializerPack&& initializers)
    {
		ScopedLock(_pimpl->_delegatesLock);
		// We need to decide whether the "typeCode" should be part of the requestHashCode
		// If it's not; then the typeCode can't impact the behaviour of the marker itself
		// (because we treat any markers with the same hash code as equivalent)
		uint64_t requestHashCode = initializers.ArchivableHash();
		auto existing = _pimpl->_markers.find(requestHashCode);
		if (existing != _pimpl->_markers.end())
			return existing->second;

		auto firstInitializer = initializers.GetInitializer<std::string>(0);		// first initializer is assumed to be a string

		for (const auto&a:_pimpl->_requestAssociations) {
			auto i = std::find(a.second._assetTypes.begin(), a.second._assetTypes.end(), typeCode);
			if (i == a.second._assetTypes.end())
				continue;
			if (std::regex_match(firstInitializer.begin(), firstInitializer.end(), a.second._regexFilter)) {
				// find the associated delegate and use that
				for (const auto&d:_pimpl->_delegates) {
					if (d.first != a.first) continue;
					auto result = std::make_shared<Marker>(std::move(initializers), d.second, _pimpl->_store);
					_pimpl->_markers.insert(std::make_pair(requestHashCode, result));
					return result;
				}
				return nullptr;
			}
		}

		return nullptr;
    }

	auto IntermediateCompilers::RegisterCompiler(
		const std::string& name,
		const std::string& shortName,
		ConsoleRig::LibVersionDesc srcVersion,
		const DepValPtr& compilerDepVal,
		CompileOperationDelegate&& delegate,
		ArchiveNameDelegate&& archiveNameDelegate
		) -> CompilerRegistration
	{
		ScopedLock(_pimpl->_delegatesLock);
		auto registration = std::make_shared<ExtensionAndDelegate>();
		auto result = _pimpl->_nextCompilerId++;
		registration->_name = name;
		registration->_srcVersion = srcVersion;
		registration->_delegate = std::move(delegate);
		registration->_archiveNameDelegate = std::move(archiveNameDelegate);
		registration->_compilerLibraryDepVal = compilerDepVal;
		if (_pimpl->_store)
			registration->_storeGroupId = _pimpl->_store->RegisterCompileProductsGroup(MakeStringSection(shortName), srcVersion, !!registration->_archiveNameDelegate);
		_pimpl->_delegates.push_back(std::make_pair(result, std::move(registration)));
		return { result };
	}

	void IntermediateCompilers::DeregisterCompiler(RegisteredCompilerId id)
	{
		ScopedLock(_pimpl->_delegatesLock);
		_pimpl->_extensionsAndDelegatesMap.erase(id);
		_pimpl->_requestAssociations.erase(id);
		for (auto i=_pimpl->_delegates.begin(); i!=_pimpl->_delegates.end();) {
			if (i->first == id) {
				i = _pimpl->_delegates.erase(i);
			} else {
				++i;
			}
		}
	}

	void IntermediateCompilers::AssociateRequest(
		RegisteredCompilerId compiler,
		IteratorRange<const uint64_t*> outputAssetTypes,
		const std::string& initializerRegexFilter)
	{
		ScopedLock(_pimpl->_delegatesLock);
		DelegateAssociation association;
		association._assetTypes = std::vector<uint64_t>{ outputAssetTypes.begin(), outputAssetTypes.end() };
		if (!initializerRegexFilter.empty())
			association._regexFilter = std::regex{initializerRegexFilter, std::regex_constants::icase};
		_pimpl->_requestAssociations.insert(std::make_pair(compiler, association));
	}

	std::vector<std::pair<std::string, std::string>> IntermediateCompilers::GetExtensionsForType(uint64_t typeCode)
	{
		std::vector<std::pair<std::string, std::string>> ext;

		ScopedLock(_pimpl->_delegatesLock);
		for (const auto&d:_pimpl->_delegates) {
			auto a = _pimpl->_requestAssociations.equal_range(d.first);
			for (auto association=a.first; association != a.second; ++association) {
				if (std::find(association->second._assetTypes.begin(), association->second._assetTypes.end(), typeCode) != association->second._assetTypes.end()) {
					// This compiler can make this type. Let's check what extensions have been registered
					auto r = _pimpl->_extensionsAndDelegatesMap.equal_range(d.first);
					for (auto i=r.first; i!=r.second; ++i)
						ext.push_back({i->second, d.second->_name});
				}
			}
		}
		return ext;
	}

	void IntermediateCompilers::RegisterExtensions(
		RegisteredCompilerId associatedCompiler,
		const std::string& commaSeparatedExtensions)
	{
		ScopedLock(_pimpl->_delegatesLock);
		auto i = commaSeparatedExtensions.begin();
		for (;;) {
			while (i != commaSeparatedExtensions.end() && (*i == ' ' || *i == '\t' || *i == ',')) ++i;
			if (i == commaSeparatedExtensions.end())
				break;

			if (*i == '.') ++i;		// if the token begins with a '.', we should just skip over and ignore it

			auto tokenBegin = i;
			auto lastNonWhitespace = commaSeparatedExtensions.end();	// set to a sentinel, to distinquish never-set from set-to-first
			while (i != commaSeparatedExtensions.end() && *i != ',') {
				if (*i != ' ' && *i != '\t') lastNonWhitespace = i;
			}
			if (lastNonWhitespace != commaSeparatedExtensions.end()) {
				_pimpl->_extensionsAndDelegatesMap.insert({associatedCompiler, std::string{tokenBegin, lastNonWhitespace+1}});
			}
		}
	}

    void IntermediateCompilers::StallOnPendingOperations(bool cancelAll)
    {
		// todo -- must reimplement, because compilation operations now occur on the main thread pool, rather than a custom thread
		assert(0);
    }

	void IntermediateCompilers::FlushCachedMarkers()
	{
		ScopedLock(_pimpl->_delegatesLock);
		_pimpl->_markers.clear();
	}

	IntermediateCompilers::IntermediateCompilers(
		const std::shared_ptr<IntermediatesStore>& store)
	{ 
		_pimpl = std::make_shared<Pimpl>(); 
		_pimpl->_store = store;
	}
	IntermediateCompilers::~IntermediateCompilers() {}

	IIntermediateCompileMarker::~IIntermediateCompileMarker() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	class CompilerLibrary
	{
	public:
		CreateCompileOperationFn* _createCompileOpFunction;
		std::shared_ptr<ConsoleRig::AttachableLibrary> _library;

		struct Kind 
		{ 
			std::vector<uint64_t> _assetTypes; 
			std::string _identifierFilter; 
			std::string _name;
			std::string _shortName;
			std::string _extensionsForOpenDlg;
		};
		std::vector<Kind> _kinds;

		CompilerLibrary(StringSection<> libraryName);
	};

	CompilerLibrary::CompilerLibrary(StringSection<> libraryName)
	: _library(std::make_shared<ConsoleRig::AttachableLibrary>(libraryName))
	{
		std::string attachErrorMsg;
		bool isAttached = _library->TryAttach(attachErrorMsg);
		if (isAttached) {
			_createCompileOpFunction = _library->GetFunction<decltype(_createCompileOpFunction)>("CreateCompileOperation");

			auto compilerDescFn = _library->GetFunction<GetCompilerDescFn*>("GetCompilerDesc");
			if (compilerDescFn) {
				auto compilerDesc = (*compilerDescFn)();
				auto targetCount = compilerDesc->FileKindCount();
				for (unsigned c=0; c<targetCount; ++c) {
					auto kind = compilerDesc->GetFileKind(c);
					_kinds.push_back({
						std::vector<uint64_t>{kind._assetTypes.begin(), kind._assetTypes.end()},
						kind._regexFilter,
						kind._name,
						kind._shortName,
						kind._extensionsForOpenDlg});
				}
			}
		}

		// check for problems (missing functions or bad version number)
		if (!isAttached)
			Throw(::Exceptions::BasicLabel("Error while attaching asset conversion DLL. Msg: (%s), from DLL: (%s)", attachErrorMsg.c_str(), libraryName.AsString().c_str()));

		if (!_createCompileOpFunction)
			Throw(::Exceptions::BasicLabel("Error while linking asset conversion DLL. Some interface functions are missing. From DLL: (%s)", libraryName.AsString().c_str()));
	}

	DirectorySearchRules DefaultLibrarySearchDirectories()
	{
		DirectorySearchRules result;
		// Default search path for libraries is just the process path.
		// In some cases (eg, for unit tests where the process path points to an internal visual studio path), 
		// we have to include extra paths
		char processPath[MaxPath];
		OSServices::GetProcessPath((utf8*)processPath, dimof(processPath));
		result.AddSearchDirectory(
			MakeFileNameSplitter(processPath).DriveAndPath());
		
		char appDir[MaxPath];
    	OSServices::GetCurrentDirectory(dimof(appDir), appDir);
		result.AddSearchDirectory(appDir);
		return result;
	}

	std::vector<IntermediateCompilers::RegisteredCompilerId> DiscoverCompileOperations(
		IntermediateCompilers& compilerManager,
		StringSection<> librarySearch,
		const DirectorySearchRules& searchRules)
	{
		std::vector<IntermediateCompilers::RegisteredCompilerId> result;

		auto candidateCompilers = searchRules.FindFiles(librarySearch);
		for (auto& c : candidateCompilers) {
			TRY {
				CompilerLibrary library{c};

				ConsoleRig::LibVersionDesc srcVersion;
				if (!library._library->TryGetVersion(srcVersion))
					Throw(std::runtime_error("Querying version returned an error"));

				std::vector<IntermediateCompilers::RegisteredCompilerId> opsFromThisLibrary;
				auto lib = library._library;
				auto fn = library._createCompileOpFunction;
				for (const auto&kind:library._kinds) {
					auto compilerDepVal = std::make_shared<DependencyValidation>();
					RegisterFileDependency(compilerDepVal, MakeStringSection(c));
					auto registrationId = compilerManager.RegisterCompiler(
						kind._name + " (" + MakeSplitPath(c).Simplify().Rebuild() + ")",
						kind._shortName,
						srcVersion,
						compilerDepVal,
						[lib, fn](auto initializers) {
							(void)lib; // hold strong reference to the library, so the DLL doesn't get unloaded
							return (*fn)(initializers);
						});

					compilerManager.AssociateRequest(
						registrationId._registrationId,
						MakeIteratorRange(kind._assetTypes),
						kind._identifierFilter);
					opsFromThisLibrary.push_back(registrationId._registrationId);
				}

				result.insert(result.end(), opsFromThisLibrary.begin(), opsFromThisLibrary.end());
			} CATCH (const std::exception& e) {
				Log(Warning) << "Failed while attempt to attach library: " << e.what() << std::endl;
			} CATCH_END
		}

		return result;
	}

	ICompileOperation::~ICompileOperation() {}
	ICompilerDesc::~ICompilerDesc() {}

}

