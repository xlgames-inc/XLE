// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GeneralCompiler.h"
#include "../../Assets/CompilationThread.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/NascentChunk.h"
#include "../../Assets/CompilerLibrary.h"
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
#include "../../Utility/StringFormat.h"
#include "../../Utility/SystemUtils.h"
#include <regex>

namespace Assets 
{
    class GeneralCompiler::Pimpl
    {
    public:
		std::vector<ExtensionAndDelegate> _delegates;

        Threading::Mutex					_threadLock;   // (used while initialising _thread for the first time)
        std::unique_ptr<::Assets::CompilationThread>	_thread;

		std::shared_ptr<IntermediateAssets::Store> _store;

		::Assets::CompilationThread& GetThread()
		{
			ScopedLock(_threadLock);
			if (!_thread)
				_thread = std::make_unique<::Assets::CompilationThread>();
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
		IteratorRange<::Assets::NascentChunk*> chunks,
        const char destinationFilename[],
        const ConsoleRig::LibVersionDesc& versionInfo)
    {
            // Create the directory if we need to...
        RawFS::CreateDirectoryRecursive(MakeFileNameSplitter(destinationFilename).DriveAndPath());

            // We need to separate out chunks that will be written to
            // the main output file from chunks that will be written to
            // a metrics file.

		if (chunks.size() == 1 && chunks[0]._hdr._type == ChunkType_Text) {
			auto outputFile = ::Assets::MainFileSystem::OpenFileInterface(destinationFilename, "wb");
			outputFile->Write(AsPointer(chunks[0]._data->begin()), chunks[0]._data->size());
		} else {
			{
				auto outputFile = ::Assets::MainFileSystem::OpenFileInterface(destinationFilename, "wb");
				BuildChunkFile(*outputFile, chunks, versionInfo, 
					[](const ::Assets::NascentChunk& c) { return c._hdr._type != ChunkType_Metrics; });
			}

			for (const auto& c:chunks)
				if (c._hdr._type == ChunkType_Metrics) {
					auto outputFile = ::Assets::MainFileSystem::OpenBasicFile(
						StringMeld<MaxPath>() << destinationFilename << "-" << c._hdr._name,
						"wb");
					outputFile.Write((const void*)AsPointer(c._data->cbegin()), 1, c._data->size());
				}
		}
    }

	static ::Assets::Blob SerializeToBlob(
		IteratorRange<::Assets::NascentChunk*> chunks,
        const ConsoleRig::LibVersionDesc& versionInfo)
	{
		if (chunks.size() == 1 && chunks[0]._hdr._type == ChunkType_Text) {
			return std::make_shared<std::vector<uint8>>(chunks[0]._data->begin(), chunks[0]._data->end());
		} else {
			auto result = std::make_shared<std::vector<uint8>>();
			auto file = ::Assets::CreateMemoryFile(result);
			BuildChunkFile(*file, chunks, versionInfo,
				[](const ::Assets::NascentChunk& c) { return c._hdr._type != ChunkType_Metrics; });
			return result;
		}
	}

	static ::Assets::DepValPtr MakeDepVal(
		IteratorRange<const ::Assets::DependentFileState*> deps,
		StringSection<::Assets::ResChar> initializer,
		StringSection<::Assets::ResChar> libraryName)
	{
		::Assets::DepValPtr depVal;
		if (!deps.empty()) {
			depVal = ::Assets::AsDepVal(deps);
		} else {
			depVal = std::make_shared<::Assets::DependencyValidation>();
			::Assets::RegisterFileDependency(depVal, MakeFileNameSplitter(initializer).AllExceptParameters());
		}
		::Assets::RegisterFileDependency(depVal, libraryName);
		return depVal;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class GeneralCompiler::Marker : public ::Assets::IArtifactCompileMarker
    {
    public:
        std::shared_ptr<::Assets::IArtifact> GetExistingAsset() const;
        std::shared_ptr<::Assets::ArtifactFuture> InvokeCompile() const;
        StringSection<::Assets::ResChar> Initializer() const;

        Marker(
            StringSection<::Assets::ResChar> requestName, uint64 typeCode,
            std::shared_ptr<GeneralCompiler> compiler);
        ~Marker();
    private:
        std::weak_ptr<GeneralCompiler> _compiler;
        ::Assets::rstring _requestName;
        uint64 _typeCode;

		static void PerformCompile(
			const GeneralCompiler::ExtensionAndDelegate& delegate,
			uint64_t typeCode, StringSection<::Assets::ResChar> initializer, 
			::Assets::ArtifactFuture& compileMarker,
			const ::Assets::IntermediateAssets::Store* destinationStore);
    };

    std::shared_ptr<::Assets::IArtifact> GeneralCompiler::Marker::GetExistingAsset() const
    {
        // ::Assets::IntermediateAssetLocator result;
        // MakeIntermediateName(result._sourceID0, dimof(result._sourceID0));
        // result._dependencyValidation = _store->MakeDependencyValidation(result._sourceID0);
        return nullptr;
    }

	void GeneralCompiler::Marker::PerformCompile(
		const GeneralCompiler::ExtensionAndDelegate& delegate,
		uint64_t typeCode, StringSection<::Assets::ResChar> initializer, 
		::Assets::ArtifactFuture& compileMarker,
		const ::Assets::IntermediateAssets::Store* destinationStore)
    {
		std::vector<::Assets::DependentFileState> deps;

        TRY
        {
            auto model = delegate._delegate(initializer);
			if (!model)
				Throw(::Exceptions::BasicLabel("Compiler library returned null to compile request on %s", initializer.AsString().c_str()));

			deps = *model->GetDependencies();

			// look for the first target of the correct type
			auto targetCount = model->TargetCount();
			bool foundTarget = false;
			for (unsigned t=0; t<targetCount; ++t) {
				auto target = model->GetTarget(t);
				if (target._type == typeCode) {
					auto chunks = model->SerializeTarget(t);

					if (destinationStore) {
						::Assets::ResChar baseIntermediateName[MaxPath];
						destinationStore->MakeIntermediateName(baseIntermediateName, (unsigned)dimof(baseIntermediateName), initializer);

						::Assets::ResChar destinationFile[MaxPath];
						XlFormatString(destinationFile, dimof(destinationFile), "%s-res-%s", baseIntermediateName, target._name);
						SerializeToFile(MakeIteratorRange(*chunks), destinationFile, delegate._srcVersion);

							// write new dependencies
						auto splitName = MakeFileNameSplitter(initializer);
						auto artifactDepVal = destinationStore->WriteDependencies(destinationFile, {}, MakeIteratorRange(deps));

						auto artifact = std::make_shared<::Assets::FileArtifact>(destinationFile, artifactDepVal);
						compileMarker.AddArtifact(target._name, artifact);
					} else {
						auto artifactDepVal = std::make_shared<::Assets::DependencyValidation>();
						::Assets::RegisterFileDependency(artifactDepVal, initializer);

						auto blob = SerializeToBlob(MakeIteratorRange(*chunks), delegate._srcVersion);
						auto artifact = std::make_shared<::Assets::BlobArtifact>(blob, artifactDepVal);
						compileMarker.AddArtifact(target._name, artifact);
					}

					foundTarget = true;
				}
			}

			if (!foundTarget)
				Throw(::Exceptions::BasicLabel("Could not find target of the requested type in compile operation for (%s)", initializer.AsString().c_str()));
        
			compileMarker.SetState(::Assets::AssetState::Ready);

        } CATCH(const ::Assets::Exceptions::ConstructionError& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, MakeDepVal(MakeIteratorRange(deps), initializer, MakeStringSection(delegate._name))));
		} CATCH(const std::exception& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, MakeDepVal(MakeIteratorRange(deps), initializer, MakeStringSection(delegate._name))));
		} CATCH(...) {
			Throw(::Assets::Exceptions::ConstructionError(
				::Assets::Exceptions::ConstructionError::Reason::Unknown,
				MakeDepVal(MakeIteratorRange(deps), initializer, MakeStringSection(delegate._name)),
				"%s", "unknown exception"));
		} CATCH_END
    }

    std::shared_ptr<::Assets::ArtifactFuture> GeneralCompiler::Marker::InvokeCompile() const
    {
        auto c = _compiler.lock();
        if (!c) return nullptr;

		auto splitRequest = MakeFileNameSplitter(_requestName);

		unsigned compilerIndex = 0;
			// Find the compiler that can handle this asset type (just by looking at the extension)
		for (; compilerIndex < c->_pimpl->_delegates.size(); ++compilerIndex) {
			const auto& filter = c->_pimpl->_delegates[compilerIndex]._extensionFilter;
			if (std::regex_match(splitRequest.Extension().begin(), splitRequest.Extension().end(), filter))
				break;
		}

		if (compilerIndex >= c->_pimpl->_delegates.size())
			Throw(::Exceptions::BasicLabel("Could not find compiler to handle request (%s)", _requestName.c_str()));

        auto backgroundOp = std::make_shared<::Assets::ArtifactFuture>();
        backgroundOp->SetInitializer(_requestName.c_str());

		auto& thread = c->_pimpl->GetThread();
		auto requestName = _requestName;
		auto typeCode = _typeCode;
		auto compiler = _compiler;
		thread.Push(
			backgroundOp,
			[compilerIndex, compiler, typeCode, requestName](::Assets::ArtifactFuture& op) {
			auto c = compiler.lock();
			if (!c) {
				op.SetState(::Assets::AssetState::Invalid);
				return;
			}

			assert(compilerIndex < c->_pimpl->_delegates.size());
			PerformCompile(c->_pimpl->_delegates[compilerIndex], typeCode, MakeStringSection(requestName), op, c->_pimpl->_store.get());
		});
        
        return std::move(backgroundOp);
    }

    StringSection<::Assets::ResChar> GeneralCompiler::Marker::Initializer() const
    {
        return MakeStringSection(_requestName);
    }

	GeneralCompiler::Marker::Marker(
        StringSection<::Assets::ResChar> requestName, uint64 typeCode,
        std::shared_ptr<GeneralCompiler> compiler)
    : _compiler(std::move(compiler)), _requestName(requestName.AsString()), _typeCode(typeCode)
    {}

	GeneralCompiler::Marker::~Marker() {}

    std::shared_ptr<::Assets::IArtifactCompileMarker> GeneralCompiler::Prepare(
        uint64 typeCode, 
        const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount)
    {
        return std::make_shared<Marker>(initializers[0], typeCode, shared_from_this());
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
			_pimpl->_delegates.emplace_back(d);
	}
	GeneralCompiler::~GeneralCompiler() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	class CompilerLibrary
	{
	public:
		::Assets::CreateCompileOperationFn* _createCompileOpFunction;
		std::shared_ptr<ConsoleRig::AttachableLibrary> _library;

		struct Kind { std::vector<uint64_t> _assetTypes; std::string _identifierFilter; };
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

			auto compilerDescFn = _library->GetFunction<::Assets::GetCompilerDescFn*>("GetCompilerDesc");
			if (compilerDescFn) {
				auto compilerDesc = (*compilerDescFn)();
				auto targetCount = compilerDesc->FileKindCount();
				for (unsigned c=0; c<targetCount; ++c) {
					auto kind = compilerDesc->GetFileKind(c);
					_kinds.push_back({
						std::vector<uint64_t>{kind._assetTypes.begin(), kind._assetTypes.end()},
						std::string{kind._extension}});
				}
			}
		}

		// check for problems (missing functions or bad version number)
		if (!isAttached)
			Throw(::Exceptions::BasicLabel("Error while attaching asset conversion DLL. Msg: (%s), from DLL: (%s)", attachErrorMsg.c_str(), libraryName.AsString().c_str()));

		if (!_createCompileOpFunction)
			Throw(::Exceptions::BasicLabel("Error while linking asset conversion DLL. Some interface functions are missing. From DLL: (%s)", libraryName.AsString().c_str()));
	}

	::Assets::DirectorySearchRules DefaultLibrarySearchDirectories()
	{
		::Assets::DirectorySearchRules result;
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
		const ::Assets::DirectorySearchRules& searchRules)
	{
		std::vector<GeneralCompiler::ExtensionAndDelegate> result;

		auto candidateCompilers = searchRules.FindFiles(MakeStringSection("*Conversion.dll"));
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
							std::regex{kind._identifierFilter}, 
							c,
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

