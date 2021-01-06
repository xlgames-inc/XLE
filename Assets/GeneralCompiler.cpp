// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GeneralCompiler.h"
#include "../../Assets/CompilationThread.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/NascentChunk.h"
#include "../../Assets/ICompileOperation.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/MemoryFile.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/DepVal.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../ConsoleRig/AttachableLibrary.h"
#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../Utility/Threading/LockFree.h"
#include "../../Utility/Threading/ThreadObject.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/Stream.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/SystemUtils.h"
#include "../../Utility/Conversion.h"
#include <regex>
#include <set>

namespace Assets 
{
    class GeneralCompiler::Pimpl
    {
    public:
		std::vector<std::shared_ptr<ExtensionAndDelegate>> _delegates;

        Threading::Mutex					_threadLock;   // (used while initialising _thread for the first time)
        std::unique_ptr<CompilationThread>	_thread;

		std::shared_ptr<IntermediateAssets::Store> _store;

		CompilationThread& GetThread()
		{
			ScopedLock(_threadLock);
			if (!_thread)
				_thread = std::make_unique<CompilationThread>();
			return *_thread;
		}

		Pimpl() {}
		Pimpl(const Pimpl&) = delete;
		Pimpl& operator=(const Pimpl&) = delete;
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
        RawFS::CreateDirectoryRecursive(MakeFileNameSplitter(destinationFilename).DriveAndPath());

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

    class GeneralCompiler::Marker : public IArtifactCompileMarker
    {
    public:
        std::shared_ptr<IArtifact> GetExistingAsset() const;
        std::shared_ptr<ArtifactFuture> InvokeCompile() const;
        StringSection<ResChar> Initializer() const;

        Marker(
            StringSection<ResChar> requestName, uint64 typeCode,
            std::shared_ptr<ExtensionAndDelegate> delegate,
			std::shared_ptr<GeneralCompiler> compiler);
        ~Marker();
    private:
        std::weak_ptr<ExtensionAndDelegate> _delegate;
		std::weak_ptr<GeneralCompiler> _compiler;
        rstring _requestName;
        uint64 _typeCode;

		static void PerformCompile(
			const GeneralCompiler::ExtensionAndDelegate& delegate,
			uint64_t typeCode, StringSection<ResChar> initializer, 
			ArtifactFuture& compileMarker,
			const IntermediateAssets::Store* destinationStore);
    };

	static void MakeIntermediateName(
		ResChar destination[], size_t destinationSize,
		StringSection<> initializer,
		StringSection<> postfix,
		const IntermediateAssets::Store& store)
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
					if (XlEqString(name, u("Artifact"))) {
						result._intermediateArtifact = value.Cast<char>().AsString();
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
					product._type = Conversion::Convert<uint64_t>(eleName.Cast<char>());
					_compileProducts.push_back(product);

					if (!formatter.TryEndElement())
						Throw(Utility::FormatException("Expecting end element in CompileProductsFile", formatter.GetLocation()));
				}
				continue;

			case FormatterBlob::AttributeName:
				Throw(Utility::FormatException("Unexpected attribute in CompileProductsFile", formatter.GetLocation()));

			default:
				/* intentional fall-through... */
			}
			break;
		}
	}

	CompileProductsFile::CompileProductsFile() {}
	CompileProductsFile::~CompileProductsFile() {}

    std::shared_ptr<IArtifact> GeneralCompiler::Marker::GetExistingAsset() const
    {
		auto c = _compiler.lock();
        if (!c) return nullptr;

		// Look for a compile products file from a previous compilation operation
		// todo -- we should store these compile product tables in an ArchiveCache
		ResChar intermediateName[MaxPath];
		MakeIntermediateName(
			intermediateName, dimof(intermediateName),
			Initializer(),  "compileprod",
			*c->_pimpl->_store);

		size_t fileSize;
		auto fileData = TryLoadFileAsMemoryBlock(intermediateName, &fileSize);
		if (!fileData) return nullptr;

		InputStreamFormatter<utf8> formatter{
			MemoryMappedInputStream{fileData.get(), PtrAdd(fileData.get(), fileSize)}};
		CompileProductsFile compileProducts(formatter);

		auto *product = compileProducts.FindProduct(_typeCode);
		if (!product) return nullptr;

		// we found a product, return a FileArtifact for it.
		auto depVal = c->_pimpl->_store->MakeDependencyValidation(product->_intermediateArtifact);
		return std::make_shared<FileArtifact>(product->_intermediateArtifact, depVal);
    }

	static std::pair<std::shared_ptr<DependencyValidation>, std::string> StoreCompileResults(
		const IntermediateAssets::Store& store,
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

	void GeneralCompiler::Marker::PerformCompile(
		const GeneralCompiler::ExtensionAndDelegate& delegate,
		uint64_t typeCode, StringSection<ResChar> initializer, 
		ArtifactFuture& compileMarker,
		const IntermediateAssets::Store* destinationStore)
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
				BasicFile file;
				if (MainFileSystem::TryOpen(file, compileProductsFile, "wb") == IFileSystem::IOReason::Success) {
					auto stream = OpenFileOutput(std::move(file));
					OutputStreamFormatter formatter(*stream);
					Serialize(formatter, compileProducts);
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

    std::shared_ptr<ArtifactFuture> GeneralCompiler::Marker::InvokeCompile() const
    {
        auto c = _compiler.lock();
        if (!c) return nullptr;

        auto backgroundOp = std::make_shared<ArtifactFuture>();
        backgroundOp->SetInitializer(_requestName.c_str());

		auto requestName = _requestName;
		auto typeCode = _typeCode;
		std::weak_ptr<ExtensionAndDelegate> weakDelegate = _delegate;
		std::weak_ptr<IntermediateAssets::Store> weakStore = c->_pimpl->_store;
		c->_pimpl->GetThread().Push(
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

    StringSection<ResChar> GeneralCompiler::Marker::Initializer() const
    {
        return MakeStringSection(_requestName);
    }

	GeneralCompiler::Marker::Marker(
        StringSection<ResChar> requestName, uint64 typeCode,
        std::shared_ptr<ExtensionAndDelegate> delegate,
		std::shared_ptr<GeneralCompiler> compiler)
    : _delegate(std::move(delegate)), _compiler(std::move(compiler)), _requestName(requestName.AsString()), _typeCode(typeCode)
    {}

	GeneralCompiler::Marker::~Marker() {}

    std::shared_ptr<IArtifactCompileMarker> GeneralCompiler::Prepare(
        uint64 typeCode, 
        const StringSection<ResChar> initializers[], unsigned initializerCount)
    {
		std::shared_ptr<ExtensionAndDelegate> delegate;

			// Find the compiler that can handle this asset type (just by looking at the extension)
		for (const auto&d:_pimpl->_delegates) {
			if (std::regex_match(initializers[0].begin(), initializers[0].end(), d->_regexFilter)) {
				delegate = d;
				break;
			}
		}

		if (!delegate)
			return nullptr;

        return std::make_shared<Marker>(initializers[0], typeCode, delegate, shared_from_this());
    }

	std::vector<uint64_t> GeneralCompiler::GetTypesForAsset(const StringSection<ResChar> initializers[], unsigned initializerCount)
	{
		for (const auto&d:_pimpl->_delegates)
			if (std::regex_match(initializers[0].begin(), initializers[0].end(), d->_regexFilter))
				return d->_assetTypes;
		return {};
	}

	std::vector<std::pair<std::string, std::string>> GeneralCompiler::GetExtensionsForType(uint64_t typeCode)
	{
		std::vector<std::pair<std::string, std::string>> ext;
		for (const auto&d:_pimpl->_delegates)
			if (std::find(d->_assetTypes.begin(), d->_assetTypes.end(), typeCode) != d->_assetTypes.end()) {
				auto i = d->_extensionsForOpenDlg.begin();
				for (;;) {
					while (i != d->_extensionsForOpenDlg.end() && *i == ',') ++i;
					auto end = std::find(i, d->_extensionsForOpenDlg.end(), ',');
					if (end == i) break;
					ext.push_back(std::make_pair(std::string(i, end), d->_name));
					i = end;
				}
			}
		return ext;
	}

    void GeneralCompiler::StallOnPendingOperations(bool cancelAll)
    {
        {
            ScopedLock(_pimpl->_threadLock);
            if (!_pimpl->_thread) return;
        }
        _pimpl->_thread->StallOnPendingOperations(cancelAll);
    }

	GeneralCompiler::GeneralCompiler(
		IteratorRange<const ExtensionAndDelegate*> delegates,
		const std::shared_ptr<IntermediateAssets::Store>& store)
	{ 
		_pimpl = std::make_shared<Pimpl>(); 
		_pimpl->_store = store;
		for (const auto&d:delegates)
			_pimpl->_delegates.emplace_back(std::make_shared<ExtensionAndDelegate>(d));
	}
	GeneralCompiler::~GeneralCompiler() {}

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
						std::string{kind._regexFilter},
						std::string{kind._name},
						kind._extensionsForOpenDlg ? std::string{kind._extensionsForOpenDlg} : std::string{}});
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
		XlGetProcessPath((utf8*)processPath, dimof(processPath));
		result.AddSearchDirectory(
			MakeFileNameSplitter(processPath).DriveAndPath());
		
		char appDir[MaxPath];
    	XlGetCurrentDirectory(dimof(appDir), appDir);
		result.AddSearchDirectory(appDir);
		return result;
	}

	std::vector<GeneralCompiler::ExtensionAndDelegate> DiscoverCompileOperations(
		StringSection<> librarySearch,
		const DirectorySearchRules& searchRules)
	{
		std::vector<GeneralCompiler::ExtensionAndDelegate> result;

		auto candidateCompilers = searchRules.FindFiles(librarySearch);
		for (auto& c : candidateCompilers) {
			TRY {
				CompilerLibrary library{c};

				ConsoleRig::LibVersionDesc srcVersion;
				if (!library._library->TryGetVersion(srcVersion))
					Throw(std::runtime_error("Querying version returned an error"));

				std::vector<GeneralCompiler::ExtensionAndDelegate> opsFromThisLibrary;
				auto lib = library._library;
				auto fn = library._createCompileOpFunction;
				for (const auto&kind:library._kinds) {
					opsFromThisLibrary.emplace_back(
						GeneralCompiler::ExtensionAndDelegate {
							kind._assetTypes,
							std::regex{kind._identifierFilter, std::regex_constants::icase}, 
							kind._name + " (" + c + ")",
							kind._extensionsForOpenDlg,
							srcVersion,
							[lib, fn](StringSection<> identifier) {
								(void)lib; // hold strong reference to the library, so the DLL doesn't get unloaded
								return (*fn)(identifier);
							}
						});
				}

				result.insert(result.end(), opsFromThisLibrary.begin(), opsFromThisLibrary.end());
			} CATCH (const std::exception& e) {
				Log(Warning) << "Failed while attempt to attach library: " << e.what() << std::endl;
			} CATCH_END
		}

		return result;
	}

}

