// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "IntermediateCompilers.h"
#include "AssetsCore.h"
#include "ICompileOperation.h"
#include "DepVal.h"
#include "IntermediatesStore.h"
#include "IArtifact.h"
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
		IntermediateCompilers::RegisteredCompilerId _registrationId;
		std::vector<uint64_t> _assetTypes;
		std::regex _regexFilter;
		std::string _name;
		ConsoleRig::LibVersionDesc _srcVersion;
		IntermediateCompilers::CompileOperationDelegate _delegate;
		DepValPtr _compilerLibraryDepVal;
		IntermediatesStore::CompileProductsGroupId _storeGroupId = 0;
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
        StringSection<ResChar> Initializer() const;

        Marker(
            IdentifiersList requestName, uint64_t typeCode,
            std::shared_ptr<ExtensionAndDelegate> delegate,
			std::shared_ptr<IntermediatesStore> intermediateStore);
        ~Marker();
    private:
		std::weak_ptr<ArtifactCollectionFuture> _activeFuture;
        std::weak_ptr<ExtensionAndDelegate> _delegate;
		std::shared_ptr<IntermediatesStore> _intermediateStore;
        std::vector<std::string> _requestName;
        uint64_t _typeCode;

		static void PerformCompile(
			const ExtensionAndDelegate& delegate,
			uint64_t typeCode, IdentifiersList initializers,
			ArtifactCollectionFuture& compileMarker,
			IntermediatesStore* destinationStore);
    };

    std::shared_ptr<IArtifactCollection> IntermediateCompilers::Marker::GetExistingAsset() const
    {
        if (!_intermediateStore) return nullptr;

		auto d = _delegate.lock();
		if (!d)
			return nullptr;

		std::vector<StringSection<>> initializers;
		initializers.resize(_requestName.size());
		for (size_t c=0; c<_requestName.size(); ++c)
			initializers[c] = MakeStringSection(_requestName[c]);

		return _intermediateStore->RetrieveCompileProducts(AsPointer(initializers.begin()), initializers.size(), d->_storeGroupId);
    }

	void IntermediateCompilers::Marker::PerformCompile(
		const ExtensionAndDelegate& delegate,
		uint64_t typeCode, IdentifiersList initializers, 
		ArtifactCollectionFuture& compileMarker,
		IntermediatesStore* destinationStore)
    {
		std::vector<DependentFileState> deps;
		assert(!initializers.empty());

        TRY
        {
            auto model = delegate._delegate(initializers);
			if (!model)
				Throw(::Exceptions::BasicLabel("Compiler library returned null to compile request on %s", initializers[0].AsString().c_str()));

			deps = model->GetDependencies();

			std::shared_ptr<IArtifactCollection> resultantArtifacts;
			std::vector<ICompileOperation::SerializedArtifact> artifactsForStore;

			auto targets = model->GetTargets();
			for (unsigned t=0; t<targets.size(); ++t) {
				const auto& target = targets[t];

				// If we have a destination store, we're going to run every compile operation
				// Otherwise, we only need to find the one with the correct asset type
				if (!destinationStore && target._type != typeCode)
					continue;

				auto chunks = model->SerializeTarget(t);

				// Note that the resultant artifacts only contains results from this 
				// one serialized target
				if (target._type == typeCode && !resultantArtifacts)
					resultantArtifacts = std::make_shared<BlobArtifactCollection>(
						MakeIteratorRange(chunks), 
						::Assets::MakeDepVal(MakeIteratorRange(deps), delegate._compilerLibraryDepVal));

				if (destinationStore)
					artifactsForStore.insert(artifactsForStore.begin(), chunks.begin(), chunks.end());
			}

			// Write out the intermediate file that lists the products of this compile operation
			if (destinationStore) {
				destinationStore->StoreCompileProducts(
					initializers.begin(), initializers.size(),
					delegate._storeGroupId,
					MakeIteratorRange(artifactsForStore),
					resultantArtifacts->GetAssetState(),
					MakeIteratorRange(deps),
					delegate._srcVersion);
			}

			if (!resultantArtifacts)
				Throw(::Exceptions::BasicLabel("Could not find target of the requested type in compile operation for (%s)", initializers[0].AsString().c_str()));
        
			compileMarker.SetArtifactCollection(resultantArtifacts);

        } CATCH(const Exceptions::ConstructionError& e) {
			auto depVal = MakeDepVal(MakeIteratorRange(deps), delegate._compilerLibraryDepVal);
			if (deps.empty())
				RegisterFileDependency(depVal, MakeFileNameSplitter(initializers[0]).AllExceptParameters());		// fallback case -- compiler might have failed because of bad input file. Interpret the initializer as a filename and create a dep val for it
			Throw(Exceptions::ConstructionError(e, depVal));
		} CATCH(const std::exception& e) {
			auto depVal = MakeDepVal(MakeIteratorRange(deps), delegate._compilerLibraryDepVal);
			if (deps.empty())
				RegisterFileDependency(depVal, MakeFileNameSplitter(initializers[0]).AllExceptParameters());
			Throw(Exceptions::ConstructionError(e, depVal));
		} CATCH(...) {
			auto depVal = MakeDepVal(MakeIteratorRange(deps), delegate._compilerLibraryDepVal);
			if (deps.empty())
				RegisterFileDependency(depVal, MakeFileNameSplitter(initializers[0]).AllExceptParameters());
			Throw(Exceptions::ConstructionError(Exceptions::ConstructionError::Reason::Unknown, depVal, "%s", "unknown exception"));
		} CATCH_END
    }

	class IntermediateCompilers::Pimpl
    {
    public:
		Threading::Mutex _delegatesLock;
		std::vector<std::shared_ptr<ExtensionAndDelegate>> _delegates;
		std::unordered_multimap<RegisteredCompilerId, std::string> _extensionsAndDelegatesMap;
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
        backgroundOp->SetInitializer(_requestName[0].c_str());

		auto requestName = _requestName;
		auto typeCode = _typeCode;
		std::weak_ptr<ExtensionAndDelegate> weakDelegate = _delegate;
		std::shared_ptr<IntermediatesStore> store = _intermediateStore;
		QueueCompileOperation(
			backgroundOp,
			[weakDelegate, store, typeCode, requestName](ArtifactCollectionFuture& op) {
			auto d = weakDelegate.lock();
			if (!d) {
				op.SetState(AssetState::Invalid);
				return;
			}

			std::vector<StringSection<>> initializers;
			initializers.resize(requestName.size());
			for (size_t c=0; c<requestName.size(); ++c)
				initializers[c] = MakeStringSection(requestName[c]);
			PerformCompile(*d, typeCode, MakeIteratorRange(initializers), op, store.get());
		});
        
		_activeFuture = backgroundOp;
        return std::move(backgroundOp);
    }

    StringSection<ResChar> IntermediateCompilers::Marker::Initializer() const
    {
        return MakeStringSection(_requestName[0]);
    }

	IntermediateCompilers::Marker::Marker(
        IdentifiersList requestName, uint64_t typeCode,
        std::shared_ptr<ExtensionAndDelegate> delegate,
		std::shared_ptr<IntermediatesStore> intermediateStore)
    : _delegate(std::move(delegate)), _intermediateStore(std::move(intermediateStore))
	, _typeCode(typeCode)
    {
		_requestName.reserve(requestName.size());
		for (const auto&n:requestName)
			_requestName.push_back(n.AsString());
	}

	IntermediateCompilers::Marker::~Marker() {}

    std::shared_ptr<IIntermediateCompileMarker> IntermediateCompilers::Prepare(
        uint64_t typeCode, 
        const StringSection<ResChar> initializers[], unsigned initializerCount)
    {
		ScopedLock(_pimpl->_delegatesLock);
		uint64_t requestHashCode = typeCode;
		for (auto c=0; c<initializerCount; ++c)
			requestHashCode = Hash64(initializers[c], requestHashCode);
		auto existing = _pimpl->_markers.find(requestHashCode);
		if (existing != _pimpl->_markers.end())
			return existing->second;

		for (const auto&d:_pimpl->_delegates) {
			auto i = std::find(d->_assetTypes.begin(), d->_assetTypes.end(), typeCode);
			if (i == d->_assetTypes.end())
				continue;
			if (std::regex_match(initializers[0].begin(), initializers[0].end(), d->_regexFilter)) {
				auto result = std::make_shared<Marker>(
					MakeIteratorRange(initializers, &initializers[initializerCount]), 
					typeCode, d, _pimpl->_store);
				_pimpl->_markers.insert(std::make_pair(requestHashCode, result));
				return result;
			}
		}

		return nullptr;
    }

	auto IntermediateCompilers::RegisterCompiler(
		const std::string& initializerRegexFilter,
		IteratorRange<const uint64_t*> outputAssetTypes,
		const std::string& name,
		ConsoleRig::LibVersionDesc srcVersion,
		const DepValPtr& compilerDepVal,
		CompileOperationDelegate&& delegate
		) -> CompilerRegistration
	{
		ScopedLock(_pimpl->_delegatesLock);
		auto registration = std::make_shared<ExtensionAndDelegate>();
		auto result = registration->_registrationId = _pimpl->_nextCompilerId++;
		registration->_assetTypes = std::vector<uint64_t>{ outputAssetTypes.begin(), outputAssetTypes.end() };
		if (!initializerRegexFilter.empty())
			registration->_regexFilter = std::regex{initializerRegexFilter, std::regex_constants::icase};
		registration->_name = name;
		registration->_srcVersion = srcVersion;
		registration->_delegate = std::move(delegate);
		registration->_compilerLibraryDepVal = compilerDepVal;
		if (_pimpl->_store)
			registration->_storeGroupId = _pimpl->_store->RegisterCompileProductsGroup(MakeStringSection(name));
		_pimpl->_delegates.push_back(registration);
		return { result };
	}

	void IntermediateCompilers::DeregisterCompiler(RegisteredCompilerId id)
	{
		ScopedLock(_pimpl->_delegatesLock);
		_pimpl->_extensionsAndDelegatesMap.erase(id);
		for (auto i=_pimpl->_delegates.begin(); i!=_pimpl->_delegates.end();) {
			if ((*i)->_registrationId == id) {
				i = _pimpl->_delegates.erase(i);
			} else {
				++i;
			}
		}
	}

	std::vector<std::pair<std::string, std::string>> IntermediateCompilers::GetExtensionsForType(uint64_t typeCode)
	{
		std::vector<std::pair<std::string, std::string>> ext;

		ScopedLock(_pimpl->_delegatesLock);
		for (const auto&d:_pimpl->_delegates)
			if (std::find(d->_assetTypes.begin(), d->_assetTypes.end(), typeCode) != d->_assetTypes.end()) {
				// This compiler can make this type. Let's check what extensions have been registered
				auto r = _pimpl->_extensionsAndDelegatesMap.equal_range(d->_registrationId);
				for (auto i=r.first; i!=r.second; ++i)
					ext.push_back({i->second, d->_name});
			}
		return ext;
	}

	void IntermediateCompilers::RegisterExtensions(
		const std::string& commaSeparatedExtensions, 
		RegisteredCompilerId associatedCompiler)
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
						kind._identifierFilter,
						MakeIteratorRange(kind._assetTypes),
						kind._name + " (" + c + ")",
						srcVersion,
						compilerDepVal,
						[lib, fn](auto initializers) {
							(void)lib; // hold strong reference to the library, so the DLL doesn't get unloaded
							return (*fn)(initializers);
						});
					opsFromThisLibrary.push_back(registrationId._registrationId);
				}

				result.insert(result.end(), opsFromThisLibrary.begin(), opsFromThisLibrary.end());
			} CATCH (const std::exception& e) {
				Log(Warning) << "Failed while attempt to attach library: " << e.what() << std::endl;
			} CATCH_END
		}

		return result;
	}

}

