// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "IntermediateCompilers.h"
#include "AssetUtils.h"
#include "AssetServices.h"
#include "AssetsCore.h"
// #include "NascentChunk.h"
#include "ICompileOperation.h"
#include "IFileSystem.h"
#include "MemoryFile.h"
#include "CompileAndAsyncManager.h"
#include "DepVal.h"
#include "IntermediatesStore.h"
#include "IArtifact.h"
#include "../ConsoleRig/AttachableLibrary.h"
#include "../OSServices/Log.h"
#include "../OSServices/LegacyFileStreams.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../Utility/Threading/LockFree.h"
#include "../Utility/Streams/PathUtils.h"
#include "../OSServices/RawFS.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Streams/Stream.h"
#include "../Utility/StringFormat.h"
#include "../OSServices/RawFS.h"
#include "../Utility/Conversion.h"
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

/*
    static const auto ChunkType_Metrics = ConstHash64<'Metr', 'ics'>::Value;
	static const auto ChunkType_Text = ConstHash64<'Text'>::Value;

    static void SerializeToFile(
		IteratorRange<const ICompileOperation::SerializedArtifact*> chunksInput,
        const char destinationFilename[],
        const ConsoleRig::LibVersionDesc& versionInfo)
    {
            // Create the directory if we need to...
        OSServices::CreateDirectoryRecursive(MakeFileNameSplitter(destinationFilename).DriveAndPath());

            // We need to separate out chunks that will be written to
            // the main output file from chunks that will be written to
            // a metrics file.

		std::vector<ICompileOperation::SerializedArtifact> chunks(chunksInput.begin(), chunksInput.end());

		for (auto c=chunks.begin(); c!=chunks.end();)
			if (c->_type == ChunkType_Metrics) {
				auto outputFile = MainFileSystem::OpenBasicFile(
					StringMeld<MaxPath>() << destinationFilename << "-" << c->_name,
					"wb");
				outputFile.Write((const void*)AsPointer(c->_data->cbegin()), 1, c->_data->size());
				c = chunks.erase(c);
			} else
				++c;

		if (chunks.size() == 1 && chunks[0]._type == ChunkType_Text) {
			auto outputFile = MainFileSystem::OpenFileInterface(destinationFilename, "wb");
			outputFile->Write(AsPointer(chunks[0]._data->begin()), chunks[0]._data->size());
		} else {
			auto outputFile = MainFileSystem::OpenFileInterface(destinationFilename, "wb");
			BuildChunkFile(*outputFile, MakeIteratorRange(chunks), versionInfo);
		}
    }
*/

	static DepValPtr MakeDepVal(
		IteratorRange<const DependentFileState*> deps,
		const DepValPtr& compilerDepVal)
	{
		DepValPtr depVal = AsDepVal(deps);
		if (compilerDepVal)
			RegisterAssetDependency(depVal, compilerDepVal);
		return depVal;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class IntermediateCompilers::Marker : public IIntermediateCompileMarker
    {
    public:
        std::shared_ptr<IArtifactCollection> GetExistingAsset() const;
        std::shared_ptr<ArtifactCollectionFuture> InvokeCompile();
        StringSection<ResChar> Initializer() const;

        Marker(
            StringSection<ResChar> requestName, uint64_t typeCode,
            std::shared_ptr<ExtensionAndDelegate> delegate,
			std::shared_ptr<IntermediatesStore> intermediateStore,
			IntermediatesStore::CompileProductsGroupId groupIdInStore);
        ~Marker();
    private:
		std::weak_ptr<ArtifactCollectionFuture> _activeFuture;
        std::weak_ptr<ExtensionAndDelegate> _delegate;
		std::shared_ptr<IntermediatesStore> _intermediateStore;
        rstring _requestName;
        uint64_t _typeCode;
		IntermediatesStore::CompileProductsGroupId _groupIdInStore;

		static void PerformCompile(
			const ExtensionAndDelegate& delegate,
			uint64_t typeCode, StringSection<ResChar> initializer, 
			ArtifactCollectionFuture& compileMarker,
			IntermediatesStore* destinationStore);
    };

/*
	static void MakeIntermediateName(
		ResChar destination[], size_t destinationSize,
		StringSection<> initializer,
		StringSection<> postfix,
		const IntermediatesStore& store)
    {
		store.MakeIntermediateName(destination, (unsigned)destinationSize, initializer);
		XlCatString(destination, destinationSize, "-");
		XlCatString(destination, destinationSize, postfix);
    }
*/

    std::shared_ptr<IArtifactCollection> IntermediateCompilers::Marker::GetExistingAsset() const
    {
        if (!_intermediateStore) return nullptr;

		// Look for a compile products file from a previous compilation operation
		// todo -- we should store these compile product tables in an ArchiveCache
		/*ResChar intermediateName[MaxPath];
		MakeIntermediateName(
			intermediateName, dimof(intermediateName),
			Initializer(),  "compileprod",
			*_intermediateStore);

		size_t fileSize;
		auto fileData = TryLoadFileAsMemoryBlock(intermediateName, &fileSize);
		if (!fileData) return nullptr;

		InputStreamFormatter<utf8> formatter{
			MemoryMappedInputStream{fileData.get(), PtrAdd(fileData.get(), fileSize)}};
		CompileProductsFile compileProducts(formatter);

		auto *product = compileProducts.FindProduct(_typeCode);
		if (!product) return nullptr;

		// we found a product, return a FileArtifact for it.
		auto depVal = _intermediateStore->MakeDependencyValidation(product->_intermediateArtifact);
		return std::make_shared<ChunkFileArtifactCollection>(
			MainFileSystem::OpenFileInterface(product->_intermediateArtifact, "rb"),
			depVal);*/

		StringSection<> initializers[] = {
			Initializer()
		};

		return _intermediateStore->RetrieveCompileProducts(initializers, dimof(initializers), _groupIdInStore);
    }

	/*static std::pair<std::shared_ptr<DependencyValidation>, std::string> StoreCompileResults(
		const IntermediatesStore& store,
		IteratorRange<const ICompileOperation::SerializedArtifact*> chunks,
		StringSection<> initializer,
		StringSection<> targetName,
		ConsoleRig::LibVersionDesc srcVersion,
		IteratorRange<const DependentFileState*> deps)
	{
		ResChar intermediateName[MaxPath];
		MakeIntermediateName(intermediateName, dimof(intermediateName), initializer, targetName, store);
		SerializeToFile(chunks, intermediateName, srcVersion);

		// write new dependencies
		auto artifactDepVal = store.WriteDependencies(intermediateName, {}, deps);
		return {artifactDepVal, intermediateName};
	}*/

	void IntermediateCompilers::Marker::PerformCompile(
		const ExtensionAndDelegate& delegate,
		uint64_t typeCode, StringSection<ResChar> initializer, 
		ArtifactCollectionFuture& compileMarker,
		IntermediatesStore* destinationStore)
    {
		std::vector<DependentFileState> deps;

        TRY
        {
            auto model = delegate._delegate(initializer);
			if (!model)
				Throw(::Exceptions::BasicLabel("Compiler library returned null to compile request on %s", initializer.AsString().c_str()));

			deps = model->GetDependencies();

			// CompileProductsFile compileProducts;
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

				/*if (destinationStore) {
					// Write the compile result to an intermediate file
					std::shared_ptr<DependencyValidation> intermediateDepVal;
					std::string intermediateName;
					std::tie(intermediateDepVal, intermediateName) = StoreCompileResults(
						*destinationStore,
						MakeIteratorRange(chunks), initializer, target._name,
						delegate._srcVersion, MakeIteratorRange(deps));

					compileProducts._compileProducts.push_back(
						CompileProductsFile::Product{target._type, intermediateName});
				}*/
			}

			// Write out the intermediate file that lists the products of this compile operation
			if (destinationStore) {
				/*
				ResChar compileProductsFile[MaxPath];
				MakeIntermediateName(
					compileProductsFile, dimof(compileProductsFile),
					initializer,  "compileprod",
					*destinationStore);
				OSServices::BasicFile file;
				if (MainFileSystem::TryOpen(file, compileProductsFile, "wb") == IFileSystem::IOReason::Success) {
					auto stream = OSServices::Legacy::OpenFileOutput(std::move(file));
					OutputStreamFormatter formatter(*stream);
					SerializationOperator(formatter, compileProducts);
				} else {
					Throw(::Exceptions::BasicLabel("Failed while attempting to write compile products file in compile operation for (%s)", initializer.AsString().c_str()));
				}
				*/
				StringSection<> initializers[] = { initializer };
				destinationStore->StoreCompileProducts(
					initializers, dimof(initializers),
					delegate._storeGroupId,
					MakeIteratorRange(artifactsForStore),
					MakeIteratorRange(deps),
					delegate._srcVersion);
			}

			if (!resultantArtifacts)
				Throw(::Exceptions::BasicLabel("Could not find target of the requested type in compile operation for (%s)", initializer.AsString().c_str()));
        
			compileMarker.SetArtifactCollection(AssetState::Ready, resultantArtifacts);

        } CATCH(const Exceptions::ConstructionError& e) {
			auto depVal = MakeDepVal(MakeIteratorRange(deps), delegate._compilerLibraryDepVal);
			if (deps.empty())
				RegisterFileDependency(depVal, MakeFileNameSplitter(initializer).AllExceptParameters());		// fallback case -- compiler might have failed because of bad input file. Interpret the initializer as a filename and create a dep val for it
			Throw(Exceptions::ConstructionError(e, depVal));
		} CATCH(const std::exception& e) {
			auto depVal = MakeDepVal(MakeIteratorRange(deps), delegate._compilerLibraryDepVal);
			if (deps.empty())
				RegisterFileDependency(depVal, MakeFileNameSplitter(initializer).AllExceptParameters());
			Throw(Exceptions::ConstructionError(e, depVal));
		} CATCH(...) {
			auto depVal = MakeDepVal(MakeIteratorRange(deps), delegate._compilerLibraryDepVal);
			if (deps.empty())
				RegisterFileDependency(depVal, MakeFileNameSplitter(initializer).AllExceptParameters());
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
        backgroundOp->SetInitializer(_requestName.c_str());

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

			PerformCompile(*d, typeCode, MakeStringSection(requestName), op, store.get());
		});
        
		_activeFuture = backgroundOp;
        return std::move(backgroundOp);
    }

    StringSection<ResChar> IntermediateCompilers::Marker::Initializer() const
    {
        return MakeStringSection(_requestName);
    }

	IntermediateCompilers::Marker::Marker(
        StringSection<ResChar> requestName, uint64_t typeCode,
        std::shared_ptr<ExtensionAndDelegate> delegate,
		std::shared_ptr<IntermediatesStore> intermediateStore,
		IntermediatesStore::CompileProductsGroupId groupIdInStore)
    : _delegate(std::move(delegate)), _intermediateStore(std::move(intermediateStore))
	, _requestName(requestName.AsString()), _typeCode(typeCode)
	, _groupIdInStore(groupIdInStore)
    {}

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

		for (const auto&d:_pimpl->_delegates)
			if (std::regex_match(initializers[0].begin(), initializers[0].end(), d->_regexFilter)) {
				auto result = std::make_shared<Marker>(initializers[0], typeCode, d, _pimpl->_store, d->_storeGroupId);
				_pimpl->_markers.insert(std::make_pair(requestHashCode, result));
				return result;
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
						[lib, fn](StringSection<> identifier) {
							(void)lib; // hold strong reference to the library, so the DLL doesn't get unloaded
							return (*fn)(identifier);
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

