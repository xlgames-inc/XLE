// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "IntermediateCompilers.h"
#include "AssetUtils.h"
#include "AssetServices.h"
#include "AssetsCore.h"
#include "NascentChunk.h"
#include "ICompileOperation.h"
#include "IFileSystem.h"
#include "MemoryFile.h"
#include "CompileAndAsyncManager.h"
#include "DepVal.h"
#include "IntermediateAssets.h"
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
	};

///////////////////////////////////////////////////////////////////////////////////////////////////

    static const auto ChunkType_Metrics = ConstHash64<'Metr', 'ics'>::Value;
	static const auto ChunkType_Text = ConstHash64<'Text'>::Value;

    static void SerializeToFile(
		IteratorRange<const ICompileOperation::OperationResult*> chunksInput,
        const char destinationFilename[],
        const ConsoleRig::LibVersionDesc& versionInfo)
    {
            // Create the directory if we need to...
        OSServices::CreateDirectoryRecursive(MakeFileNameSplitter(destinationFilename).DriveAndPath());

            // We need to separate out chunks that will be written to
            // the main output file from chunks that will be written to
            // a metrics file.

		std::vector<ICompileOperation::OperationResult> chunks(chunksInput.begin(), chunksInput.end());

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

	static Blob SerializeToBlob(
		IteratorRange<const ICompileOperation::OperationResult*> chunks,
        const ConsoleRig::LibVersionDesc& versionInfo)
	{
		if (chunks.size() == 1 && chunks[0]._type == ChunkType_Text) {
			return chunks[0]._data;
		} else {
			auto result = std::make_shared<std::vector<uint8>>();
			auto file = CreateMemoryFile(result);
			BuildChunkFile(*file, chunks, versionInfo,
				[](const ICompileOperation::OperationResult& c) { return c._type != ChunkType_Metrics; });
			return result;
		}
	}

	static DepValPtr MakeDepVal(
		IteratorRange<const DependentFileState*> deps,
		StringSection<ResChar> initializer,
		StringSection<ResChar> libraryName)
	{
		DepValPtr depVal;
		if (!deps.empty()) {
			depVal = AsDepVal(deps);
		} else {
			depVal = std::make_shared<DependencyValidation>();
			RegisterFileDependency(depVal, MakeFileNameSplitter(initializer).AllExceptParameters());
		}
		RegisterFileDependency(depVal, libraryName);
		return depVal;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class IntermediateCompilers::Marker : public IArtifactCompileMarker
    {
    public:
        std::shared_ptr<IArtifact> GetExistingAsset() const;
        std::shared_ptr<ArtifactFuture> InvokeCompile() const;
        StringSection<ResChar> Initializer() const;

        Marker(
            StringSection<ResChar> requestName, uint64_t typeCode,
            std::shared_ptr<ExtensionAndDelegate> delegate,
			std::shared_ptr<IntermediatesStore> intermediateStore);
        ~Marker();
    private:
        std::weak_ptr<ExtensionAndDelegate> _delegate;
		std::weak_ptr<IntermediatesStore> _intermediateStore;
        rstring _requestName;
        uint64_t _typeCode;

		static void PerformCompile(
			const ExtensionAndDelegate& delegate,
			uint64_t typeCode, StringSection<ResChar> initializer, 
			ArtifactFuture& compileMarker,
			const IntermediatesStore* destinationStore);
    };

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

	class CompileProductsFile
	{
	public:
		struct Product
		{
			uint64_t _type;
			std::string _intermediateArtifact;
		};
		std::vector<Product> _compileProducts;

		const Product* FindProduct(uint64_t type) const
		{
			for (const auto&p:_compileProducts)
				if (p._type == type)
					return &p;
			return nullptr;
		}

		void SerializeMethod(OutputStreamFormatter& formatter) const;
		CompileProductsFile();
		CompileProductsFile(InputStreamFormatter<utf8>& formatter);
		~CompileProductsFile();
	};

	void CompileProductsFile::SerializeMethod(OutputStreamFormatter& formatter) const
	{
		for (const auto&product:_compileProducts) {
			auto ele = formatter.BeginElement(std::to_string(product._type));
			formatter.WriteAttribute("Artifact", product._intermediateArtifact.c_str());
			formatter.EndElement(ele);
		}
	}

	static CompileProductsFile::Product SerializeProduct(InputStreamFormatter<utf8>& formatter)
	{
		CompileProductsFile::Product result;
		using FormatterBlob = InputStreamFormatter<utf8>::Blob;
		for (;;) {
			switch (formatter.PeekNext()) {
			case FormatterBlob::AttributeName:
				{
					StringSection<utf8> name, value;
					if (!formatter.TryAttribute(name, value))
						Throw(Utility::FormatException("Poorly formed attribute in CompileProductsFile", formatter.GetLocation()));
					if (XlEqString(name, "Artifact")) {
						result._intermediateArtifact = value.AsString();
					} else
						Throw(Utility::FormatException("Unknown attribute in CompileProductsFile", formatter.GetLocation()));
				}
				continue;

			case FormatterBlob::EndElement:
				break;

			default:
				Throw(Utility::FormatException("Unexpected blob in CompileProductsFile", formatter.GetLocation()));
			}
			break;
		}
		return result;
	}

	CompileProductsFile::CompileProductsFile(InputStreamFormatter<utf8>& formatter)
	{
		using FormatterBlob = InputStreamFormatter<utf8>::Blob;
		for (;;) {
			switch (formatter.PeekNext()) {
			case FormatterBlob::BeginElement:
				{
					InputStreamFormatter<utf8>::InteriorSection eleName;
					if (!formatter.TryBeginElement(eleName))
						Throw(Utility::FormatException("Poorly formed begin element in CompileProductsFile", formatter.GetLocation()));

					auto product = SerializeProduct(formatter);
					product._type = Conversion::Convert<uint64_t>(eleName);
					_compileProducts.push_back(product);

					if (!formatter.TryEndElement())
						Throw(Utility::FormatException("Expecting end element in CompileProductsFile", formatter.GetLocation()));
				}
				continue;

			case FormatterBlob::AttributeName:
				Throw(Utility::FormatException("Unexpected attribute in CompileProductsFile", formatter.GetLocation()));

			default:
				break;
			}
			break;
		}
	}

	CompileProductsFile::CompileProductsFile() {}
	CompileProductsFile::~CompileProductsFile() {}

    std::shared_ptr<IArtifact> IntermediateCompilers::Marker::GetExistingAsset() const
    {
		auto st = _intermediateStore.lock();
        if (!st) return nullptr;

		// Look for a compile products file from a previous compilation operation
		// todo -- we should store these compile product tables in an ArchiveCache
		ResChar intermediateName[MaxPath];
		MakeIntermediateName(
			intermediateName, dimof(intermediateName),
			Initializer(),  "compileprod",
			*st);

		size_t fileSize;
		auto fileData = TryLoadFileAsMemoryBlock(intermediateName, &fileSize);
		if (!fileData) return nullptr;

		InputStreamFormatter<utf8> formatter{
			MemoryMappedInputStream{fileData.get(), PtrAdd(fileData.get(), fileSize)}};
		CompileProductsFile compileProducts(formatter);

		auto *product = compileProducts.FindProduct(_typeCode);
		if (!product) return nullptr;

		// we found a product, return a FileArtifact for it.
		auto depVal = st->MakeDependencyValidation(product->_intermediateArtifact);
		return std::make_shared<FileArtifact>(product->_intermediateArtifact, depVal);
    }

	static std::pair<std::shared_ptr<DependencyValidation>, std::string> StoreCompileResults(
		const IntermediatesStore& store,
		IteratorRange<const ICompileOperation::OperationResult*> chunks,
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
	}

	void IntermediateCompilers::Marker::PerformCompile(
		const ExtensionAndDelegate& delegate,
		uint64_t typeCode, StringSection<ResChar> initializer, 
		ArtifactFuture& compileMarker,
		const IntermediatesStore* destinationStore)
    {
		std::vector<DependentFileState> deps;

        TRY
        {
            auto model = delegate._delegate(initializer);
			if (!model)
				Throw(::Exceptions::BasicLabel("Compiler library returned null to compile request on %s", initializer.AsString().c_str()));

			deps = model->GetDependencies();

			CompileProductsFile compileProducts;

			auto targets = model->GetTargets();
			bool foundTarget = false;
			for (unsigned t=0; t<targets.size(); ++t) {
				const auto& target = targets[t];

				// If we have a destination store, we're going to run every compile operation
				// Otherwise, we only need to find the one with the correct asset type
				if (!destinationStore && target._type != typeCode)
					continue;

				auto chunks = model->SerializeTarget(t);
				if (destinationStore) {
					// Write the compile result to an intermediate file
					std::shared_ptr<DependencyValidation> intermediateDepVal;
					std::string intermediateName;
					std::tie(intermediateDepVal, intermediateName) = StoreCompileResults(
						*destinationStore,
						MakeIteratorRange(chunks), initializer, target._name,
						delegate._srcVersion, MakeIteratorRange(deps));

					compileProducts._compileProducts.push_back(
						CompileProductsFile::Product{target._type, intermediateName});

					// If this is the particular artifact we originally requested, we should create an artifact for it
					if  (target._type == typeCode && !foundTarget) {
						auto artifact = std::make_shared<FileArtifact>(intermediateName, intermediateDepVal);
						compileMarker.AddArtifact(target._name, artifact);
						foundTarget = true;
					}
				} else if (!foundTarget) {
					auto artifactDepVal = std::make_shared<DependencyValidation>();
					RegisterFileDependency(artifactDepVal, initializer);

					auto blob = SerializeToBlob(MakeIteratorRange(chunks), delegate._srcVersion);
					auto artifact = std::make_shared<BlobArtifact>(blob, artifactDepVal);
					compileMarker.AddArtifact(target._name, artifact);
					foundTarget = true;
					break;
				}
			}

			// Write out the intermediate file that lists the products of this compile operation
			if (destinationStore) {
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
			}

			if (!foundTarget)
				Throw(::Exceptions::BasicLabel("Could not find target of the requested type in compile operation for (%s)", initializer.AsString().c_str()));
        
			compileMarker.SetState(AssetState::Ready);

        } CATCH(const Exceptions::ConstructionError& e) {
			Throw(Exceptions::ConstructionError(e, MakeDepVal(MakeIteratorRange(deps), initializer, MakeStringSection(delegate._name))));
		} CATCH(const std::exception& e) {
			Throw(Exceptions::ConstructionError(e, MakeDepVal(MakeIteratorRange(deps), initializer, MakeStringSection(delegate._name))));
		} CATCH(...) {
			Throw(Exceptions::ConstructionError(
				Exceptions::ConstructionError::Reason::Unknown,
				MakeDepVal(MakeIteratorRange(deps), initializer, MakeStringSection(delegate._name)),
				"%s", "unknown exception"));
		} CATCH_END
    }

	class IntermediateCompilers::Pimpl
    {
    public:
		Threading::Mutex _delegatesLock;
		std::vector<std::shared_ptr<ExtensionAndDelegate>> _delegates;
		std::unordered_multimap<RegisteredCompilerId, std::string> _extensionsAndDelegatesMap;
		std::shared_ptr<IntermediatesStore> _store;
		RegisteredCompilerId _nextCompilerId = 1;

		Pimpl() {}
		Pimpl(const Pimpl&) = delete;
		Pimpl& operator=(const Pimpl&) = delete;
    };

    std::shared_ptr<ArtifactFuture> IntermediateCompilers::Marker::InvokeCompile() const
    {
        auto backgroundOp = std::make_shared<ArtifactFuture>();
        backgroundOp->SetInitializer(_requestName.c_str());

		auto requestName = _requestName;
		auto typeCode = _typeCode;
		std::weak_ptr<ExtensionAndDelegate> weakDelegate = _delegate;
		std::weak_ptr<IntermediatesStore> weakStore = _intermediateStore;
		QueueCompileOperation(
			backgroundOp,
			[weakDelegate, weakStore, typeCode, requestName](ArtifactFuture& op) {
			auto d = weakDelegate.lock();
			auto s = weakStore.lock();
			if (!d || !s) {
				op.SetState(AssetState::Invalid);
				return;
			}

			PerformCompile(*d, typeCode, MakeStringSection(requestName), op, s.get());
		});
        
        return std::move(backgroundOp);
    }

    StringSection<ResChar> IntermediateCompilers::Marker::Initializer() const
    {
        return MakeStringSection(_requestName);
    }

	IntermediateCompilers::Marker::Marker(
        StringSection<ResChar> requestName, uint64_t typeCode,
        std::shared_ptr<ExtensionAndDelegate> delegate,
		std::shared_ptr<IntermediatesStore> intermediateStore)
    : _delegate(std::move(delegate)), _intermediateStore(std::move(intermediateStore))
	, _requestName(requestName.AsString()), _typeCode(typeCode)
    {}

	IntermediateCompilers::Marker::~Marker() {}

    std::shared_ptr<IArtifactCompileMarker> IntermediateCompilers::Prepare(
        uint64_t typeCode, 
        const StringSection<ResChar> initializers[], unsigned initializerCount)
    {
		ScopedLock(_pimpl->_delegatesLock);
		for (const auto&d:_pimpl->_delegates)
			if (std::regex_match(initializers[0].begin(), initializers[0].end(), d->_regexFilter))
				return std::make_shared<Marker>(initializers[0], typeCode, d, _pimpl->_store);

		return nullptr;
    }

	auto IntermediateCompilers::RegisterCompiler(
		const std::string& initializerRegexFilter,
		IteratorRange<const uint64_t*> outputAssetTypes,
		const std::string& name,
		ConsoleRig::LibVersionDesc srcVersion,
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
					auto registrationId = compilerManager.RegisterCompiler(
						kind._identifierFilter,
						MakeIteratorRange(kind._assetTypes),
						kind._name + " (" + c + ")",
						srcVersion,
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

