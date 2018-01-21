// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelCompiler.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/NascentChunk.h"
#include "../../Assets/CompilerLibrary.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/CompilationThread.h"
#include "../../Assets/MemoryFile.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/DepVal.h"
#include "../../ConsoleRig/AttachableLibrary.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/Threading/LockFree.h"
#include "../../Utility/Threading/ThreadObject.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/SystemUtils.h"

#pragma warning(disable:4505)       // warning C4505: 'RenderCore::Assets::SerializeToFile' : unreferenced local function has been removed

namespace RenderCore { namespace Assets 
{
	typedef std::shared_ptr<::Assets::ICompilerDesc> GetCompilerDescFn();
	typedef std::shared_ptr<::Assets::ICompileOperation> CreateCompileOperationFn(StringSection<::Assets::ResChar> identifier);

	class CompilerLibrary
	{
	public:
		void PerformCompile(
			uint64 typeCode, StringSection<::Assets::ResChar> initializer, 
			StringSection<::Assets::ResChar> destinationFile,
			::Assets::CompileFuture& compileMarker,
			const ::Assets::IntermediateAssets::Store& destinationStore);
		void AttachLibrary();

		bool IsKnownExtension(StringSection<::Assets::ResChar> ext)
		{
			AttachLibrary();
			for (const auto& e:_knownExtensions)
				if (XlEqStringI(MakeStringSection(e), ext)) return true;
			return false;
		}

		CompilerLibrary(StringSection<::Assets::ResChar> libraryName)
			: _library(libraryName)
		{
			_createCompileOpFunction = nullptr;
			_isAttached = _attemptedAttach = false;
		}

		CompilerLibrary(CompilerLibrary&&) never_throws = default;
		CompilerLibrary& operator=(CompilerLibrary&&) never_throws = default;

	private:
		// ---------- interface to DLL functions ----------
		CreateCompileOperationFn* _createCompileOpFunction;

		std::vector<::Assets::rstring> _knownExtensions;

		::Assets::rstring _libraryName;
		ConsoleRig::AttachableLibrary _library;
		bool _isAttached;
		bool _attemptedAttach;
	};

    class ModelCompiler::Pimpl
    {
    public:
		std::vector<CompilerLibrary>		_compilers;
		bool								_discoveryDone;
		::Assets::DirectorySearchRules		_librarySearchRules;

        Threading::Mutex					_threadLock;   // (used while initialising _thread for the first time)
        std::unique_ptr<::Assets::CompilationThread>	_thread;

		void DiscoverLibraries();

		::Assets::CompilationThread& GetThread()
		{
			ScopedLock(_threadLock);
			if (!_thread)
				_thread = std::make_unique<::Assets::CompilationThread>();
			return *_thread;
		}

		Pimpl() : _discoveryDone(false) {}
		Pimpl(const Pimpl&) = delete;
		Pimpl& operator=(const Pimpl&) = delete;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    static const auto ChunkType_Metrics = ConstHash64<'Metr', 'ics'>::Value;

    static void SerializeToFile(
		::Assets::IFileInterface& mainFile,
		IteratorRange<::Assets::NascentChunk*> chunks,
        const ConsoleRig::LibVersionDesc& versionInfo)
    {
            // We need to separate out chunks that will be written to
            // the main output file from chunks that will be written to
            // a metrics file.

        ::Assets::BuildChunkFile(mainFile, chunks, versionInfo,
            [](const ::Assets::NascentChunk& c) { return c._hdr._type != ChunkType_Metrics; });
	}

	static void WriteMetrics(
		StringSection<::Assets::ResChar> destinationFile,
		IteratorRange<::Assets::NascentChunk*> chunks)
	{
        for (const auto& c:chunks)
            if (c._hdr._type == ChunkType_Metrics) {
				auto metricsFile = ::Assets::MainFileSystem::OpenFileInterface(
					StringMeld<MaxPath>() << destinationFile << "-" << c._hdr._name,
					"wb");
				metricsFile->Write((const void*)AsPointer(c._data->cbegin()), 1, c._data->size());
            }
    }

    static void SerializeToFileJustChunk(
		::Assets::IFileInterface& mainFile,
		IteratorRange<::Assets::NascentChunk*> chunks,
        const ConsoleRig::LibVersionDesc& versionInfo)
    {
		for (const auto& c:chunks)
			mainFile.Write(AsPointer(c._data->begin()), c._data->size(), 1);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void CompilerLibrary::PerformCompile(
		uint64 typeCode, StringSection<::Assets::ResChar> initializer, 
		StringSection<::Assets::ResChar> destinationFile,
		::Assets::CompileFuture& compileMarker,
		const ::Assets::IntermediateAssets::Store& destinationStore)
    {
        TRY
        {
            AttachLibrary();

            ConsoleRig::LibVersionDesc libVersionDesc;
            _library.TryGetVersion(libVersionDesc);

			bool requiresMerge = (typeCode == ModelCompiler::Type_AnimationSet) && XlFindChar(initializer, '*');
            if (!requiresMerge) {

                auto model = (*_createCompileOpFunction)(initializer);

				auto mainBlob = std::make_shared<std::vector<uint8_t>>();
				auto mainFile = ::Assets::CreateMemoryFile(mainBlob);

				// Create the directory if we need to...
				RawFS::CreateDirectoryRecursive(MakeFileNameSplitter(destinationFile).DriveAndPath());

				// look for the first target of the correct type
				auto targetCount = model->TargetCount();
				bool foundTarget = false;
				for (unsigned t=0; t<targetCount; ++t)
					if (model->GetTarget(t)._type == typeCode) {
						auto chunks = model->SerializeTarget(t);
						if (typeCode != ModelCompiler::Type_RawMat) {
							SerializeToFile(*mainFile, MakeIteratorRange(*chunks), libVersionDesc);
						} else 
							SerializeToFileJustChunk(*mainFile, MakeIteratorRange(*chunks), libVersionDesc);
						WriteMetrics(destinationFile, MakeIteratorRange(*chunks));
						foundTarget = true;
						break;
					}

				if (!foundTarget)
					Throw(::Exceptions::BasicLabel("Could not find target of the requested type in compile operation for (%s)", initializer));

				{
					auto dst = ::Assets::MainFileSystem::OpenFileInterface(destinationFile, "wb");
					dst->Write(mainBlob->data(), mainBlob->size());
				}

				const auto& deps = *model->GetDependencies();
				auto depVal = destinationStore.WriteDependencies(destinationFile, {}, MakeIteratorRange(deps));
				::Assets::rstring requestParams;
				if (typeCode == ModelCompiler::Type_RawMat)
					requestParams = MakeFileNameSplitter(initializer).Parameters().AsString();
				compileMarker.AddArtifact(
					"main",
					std::make_shared<::Assets::BlobArtifact>(mainBlob, depVal, requestParams));

				compileMarker.SetState(::Assets::AssetState::Ready);

            }  else {

				assert(0);	// broken when generalizing animation set serialization functionality.
							// We now need to do the merging in this module; just serialize animations from single
							// source models in the converter, and deserialize from chunks and merge together here
#if 0
				if (!_extractAnimationsFn || !_serializeAnimationSetFn || !_createAnimationSetFn)
					Throw(::Exceptions::BasicLabel("Could not execute animation conversion operation because this compiler library doesn't provide an interface for animations"));

                    //  source for the animation set should actually be a directory name, and
                    //  we'll use all of the dae files in that directory as animation inputs
                auto sourceFiles = RawFS::FindFiles(initializer.AsString() + "/*.dae");
                std::vector<::Assets::DependentFileState> deps;

                auto mergedAnimationSet = (*_createAnimationSetFn)("mergedanim");
                for (auto i=sourceFiles.begin(); i!=sourceFiles.end(); ++i) {
                    char baseName[MaxPath]; // get the base name of the file (without the extension)
                    XlBasename(baseName, dimof(baseName), i->c_str());
                    XlChopExtension(baseName);

                    TRY {
                            //
                            //      First; load the animation file as a model
                            //          note that this will do geometry processing; etc -- but all that geometry
                            //          information will be ignored.
                            //
                        auto model = (*_createCompileOpFunction)(i->c_str());

                            //
                            //      Now, merge the animation data into 
                        (*_extractAnimationsFn)(*mergedAnimationSet.get(), *model.get(), baseName);
                    } CATCH (const std::exception& e) {
                            // on exception, ignore this animation file and move on to the next
                        LogAlwaysError << "Exception while processing animation: (" << baseName << "). Exception is: (" << e.what() << ")";
                    } CATCH_END

                    deps.push_back(destinationStore.GetDependentFileState(i->c_str()));
                }

				auto chunks = (*_serializeAnimationSetFn)(*mergedAnimationSet);
                SerializeToFile(MakeIteratorRange(*chunks), compileMarker.GetLocator()._sourceID0, libVersionDesc);

				compileMarker.GetLocator()._dependencyValidation = destinationStore.WriteDependencies(compileMarker.GetLocator()._sourceID0, splitName.DriveAndPath(), MakeIteratorRange(deps));
                if (::Assets::Services::GetInvalidAssetMan())
                    ::Assets::Services::GetInvalidAssetMan()->MarkValid(initializer);
				compileMarker.SetState(::Assets::AssetState::Ready);
#endif
            }
        } CATCH(const std::exception& e) {
            LogAlwaysError << "Caught exception while performing Collada conversion. Exception details as follows:";
            LogAlwaysError << e.what();
            //if (::Assets::Services::GetInvalidAssetMan())
            //    ::Assets::Services::GetInvalidAssetMan()->MarkInvalid(initializer, e.what());
			compileMarker.SetState(::Assets::AssetState::Invalid);
        } CATCH(...) {
//            if (::Assets::Services::GetInvalidAssetMan())
//                ::Assets::Services::GetInvalidAssetMan()->MarkInvalid(initializer, "Unknown error");
			compileMarker.SetState(::Assets::AssetState::Invalid);
        } CATCH_END
    }

	void CompilerLibrary::AttachLibrary()
	{
		if (!_attemptedAttach && !_isAttached) {
			_attemptedAttach = true;

			_isAttached = _library.TryAttach();
			if (_isAttached) {
				_createCompileOpFunction    = _library.GetFunction<decltype(_createCompileOpFunction)>("CreateCompileOperation");

				auto compilerDescFn = _library.GetFunction<GetCompilerDescFn*>("GetCompilerDesc");
				if (compilerDescFn) {
					auto compilerDesc = (*compilerDescFn)();
					auto targetCount = compilerDesc->FileKindCount();
					for (unsigned c=0; c<targetCount; ++c) {
						_knownExtensions.push_back(compilerDesc->GetFileKind(c)._extension);
					}
				}
			}
		}

		// check for problems (missing functions or bad version number)
		if (!_isAttached)
			Throw(::Exceptions::BasicLabel("Error while linking asset conversion DLL. Could not find DLL (%s)", _libraryName.c_str()));

		if (!_createCompileOpFunction)
			Throw(::Exceptions::BasicLabel("Error while linking asset conversion DLL. Some interface functions are missing"));
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ModelCompiler::Marker : public ::Assets::ICompileMarker
    {
    public:
        std::shared_ptr<::Assets::IArtifact> GetExistingAsset() const;
        std::shared_ptr<::Assets::CompileFuture> InvokeCompile() const;
        StringSection<::Assets::ResChar> Initializer() const;

        Marker(
            StringSection<::Assets::ResChar> requestName, uint64 typeCode,
            const ::Assets::IntermediateAssets::Store& store,
            std::shared_ptr<ModelCompiler> compiler);
        ~Marker();
    private:
        std::weak_ptr<ModelCompiler> _compiler;
        ::Assets::rstring _requestName;
        uint64 _typeCode;
        const ::Assets::IntermediateAssets::Store* _store;

        void MakeIntermediateName(::Assets::ResChar destination[], size_t count) const;
    };

    void ModelCompiler::Marker::MakeIntermediateName(::Assets::ResChar destination[], size_t count) const
    {
        bool stripParams = _typeCode == ModelCompiler::Type_RawMat;
        if (stripParams) {
            _store->MakeIntermediateName(
                destination, (unsigned)count, MakeFileNameSplitter(_requestName).AllExceptParameters());
        } else {
            _store->MakeIntermediateName(destination, (unsigned)count, MakeStringSection(_requestName));
        }

        switch (_typeCode) {
        default:
            assert(0);
        case Type_Model: XlCatString(destination, count, "-skin"); break;
        case Type_Skeleton: XlCatString(destination, count, "-skel"); break;
        case Type_AnimationSet: XlCatString(destination, count, "-anim"); break;
        case Type_RawMat: XlCatString(destination, count, "-rawmat"); break;
        }
    }

    std::shared_ptr<::Assets::IArtifact> ModelCompiler::Marker::GetExistingAsset() const
    {
		auto splitRequest = MakeFileNameSplitter(_requestName);
		if (_typeCode == ModelCompiler::Type_RawMat) {
			if (XlEqStringI(splitRequest.Extension(), "material")) {
				auto depVal = std::make_shared<::Assets::DependencyValidation>();
				RegisterFileDependency(depVal, splitRequest.AllExceptParameters());
				return std::make_shared<::Assets::FileArtifact>(_requestName, depVal);
			}
		}

		::Assets::ResChar intermediateName[MaxPath];
        MakeIntermediateName(intermediateName, dimof(intermediateName));
		auto depVal = _store->MakeDependencyValidation(intermediateName);
		if (_typeCode == ModelCompiler::Type_RawMat)
			XlCatString(intermediateName, splitRequest.ParametersWithDivider());
		return std::make_shared<::Assets::FileArtifact>(intermediateName, depVal);
    }

    std::shared_ptr<::Assets::CompileFuture> ModelCompiler::Marker::InvokeCompile() const
    {
        auto c = _compiler.lock();
        if (!c) return nullptr;

		c->_pimpl->DiscoverLibraries();

		auto splitRequest = MakeFileNameSplitter(_requestName);

		unsigned compilerIndex = 0;
			// Find the compiler that can handle this asset type (just by looking at the extension)
		for (; compilerIndex < c->_pimpl->_compilers.size(); ++compilerIndex)
			if (c->_pimpl->_compilers[compilerIndex].IsKnownExtension(splitRequest.Extension()))
				break;

		if (compilerIndex >= c->_pimpl->_compilers.size())
			Throw(::Exceptions::BasicLabel("Could not find compiler to handle request (%s)", _requestName.c_str()));

            // Queue this compilation operation to occur in the background thread.
            //
            // With the old path,  we couldn't do multiple Collada compilation at the same time. 
            // So this implementation just spawns a single dedicated threads.
            //
            // However, with the new implementation, we could use one of the global thread pools
            // and it should be ok to queue up multiple compilations at the same time.
        auto backgroundOp = std::make_shared<::Assets::CompileFuture>();
        backgroundOp->SetInitializer(_requestName.c_str());

		::Assets::ResChar intermediateName[MaxPath];
		MakeIntermediateName(intermediateName, dimof(intermediateName));

        auto& thread = c->_pimpl->GetThread();
		::Assets::rstring destinationFile = intermediateName;
		auto requestName = _requestName;
		auto typeCode = _typeCode;
		auto* store = _store;
		auto compiler = _compiler;
        thread.Push(
			backgroundOp,
			[compilerIndex, compiler, typeCode, requestName, destinationFile, store](::Assets::CompileFuture& op) {
				auto c = compiler.lock();
				if (!c) {
					op.SetState(::Assets::AssetState::Invalid);
					return;
				}

				assert(compilerIndex < c->_pimpl->_compilers.size());
				c->_pimpl->_compilers[compilerIndex].PerformCompile(typeCode, MakeStringSection(requestName), destinationFile, op, *store);
			});
        
        return std::move(backgroundOp);
    }

    StringSection<::Assets::ResChar> ModelCompiler::Marker::Initializer() const
    {
        return MakeStringSection(_requestName);
    }

    ModelCompiler::Marker::Marker(
        StringSection<::Assets::ResChar> requestName, uint64 typeCode,
        const ::Assets::IntermediateAssets::Store& store,
        std::shared_ptr<ModelCompiler> compiler)
    : _compiler(std::move(compiler)), _requestName(requestName.AsString()), _typeCode(typeCode), _store(&store)
    {}

    ModelCompiler::Marker::~Marker() {}

    std::shared_ptr<::Assets::ICompileMarker> ModelCompiler::PrepareAsset(
        uint64 typeCode, 
        const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount, 
        const ::Assets::IntermediateAssets::Store& destinationStore)
    {
        return std::make_shared<Marker>(initializers[0], typeCode, destinationStore, shared_from_this());
    }

    void ModelCompiler::StallOnPendingOperations(bool cancelAll)
    {
        {
            ScopedLock(_pimpl->_threadLock);
            if (!_pimpl->_thread) return;
        }
        _pimpl->_thread->StallOnPendingOperations(cancelAll);
    }

	void ModelCompiler::AddLibrarySearchDirectories(const ::Assets::DirectorySearchRules& directories)
	{
		assert(!_pimpl->_discoveryDone);
		_pimpl->_librarySearchRules.Merge(directories);
	}

    ModelCompiler::ModelCompiler() 
	{ 
		_pimpl = std::make_shared<Pimpl>(); 

		// Default search path for libraries is just the process path.
		// In some cases (eg, for unit tests where the process path points to an internal visual studio path), 
		// we have to include extra paths
		char processPath[MaxPath];
		XlGetProcessPath((utf8*)processPath, dimof(processPath));
		_pimpl->_librarySearchRules.AddSearchDirectory(
			MakeFileNameSplitter(processPath).DriveAndPath());
	}
    ModelCompiler::~ModelCompiler() {}

	void ModelCompiler::Pimpl::DiscoverLibraries()
	{
		if (_discoveryDone) return;

		// Look for attachable libraries that can compile raw assets
		// We're expecting to find them in the same directory as the executable with the form "*Conversion.dll" 
		auto candidateCompilers = _librarySearchRules.FindFiles(MakeStringSection("*Conversion.dll"));
		for (auto& c:candidateCompilers)
			_compilers.emplace_back(CompilerLibrary(c));

		_discoveryDone = true;
	}

}}

