// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LocalCompiledShaderSource.h"
#include "../Metal/Shader.h"

#include "../IDevice.h"
#include "../../Assets/ChunkFile.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/ArchiveCache.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/AsyncLoadOperation.h"
#include "../../Assets/IFileSystem.h"

#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Threading/CompletionThreadPool.h"
#include "../../Utility/StringFormat.h"

#include <functional>
#include <deque>
#include <regex>
#include <sstream>

namespace RenderCore { namespace Assets 
{
    #if defined(TEMP_HACK)
        static const bool CompileInBackground = true;
    #else
        static const bool CompileInBackground = false;
    #endif
    using ::Assets::ResChar;
    using ResId = ShaderService::ResId;

        ////////////////////////////////////////////////////////////

    class ShaderCompileMarker : public ::Assets::CompileFuture
        #if defined(TEMP_HACK)
            , public ::Assets::AsyncLoadOperation
        #endif
    {
    public:
        using Payload = ::Assets::Blob;
        using ChainFn = std::function<void(
			const ResId& shaderPath, const ::Assets::rstring& definesTable,
            ::Assets::AssetState, const Payload& payload, const Payload& errors,
            IteratorRange<const ::Assets::DependentFileState*>)>;

        const Payload& Resolve(StringSection<::Assets::ResChar> initializer, const ::Assets::DepValPtr& depVal = nullptr) const;
        ::Assets::AssetState TryResolve(Payload& result, const ::Assets::DepValPtr& depVal) const;
        Payload GetErrors() const;

        ::Assets::AssetState StallWhilePending() const;
		ShaderStage GetStage() const { return _shaderPath.AsShaderStage(); }

        const std::vector<::Assets::DependentFileState>& GetDependencies() const;

        void Enqueue(
            const ResId& shaderPath, const ::Assets::rstring& definesTable, 
            ChainFn chain = nullptr,
            const std::shared_ptr<::Assets::DependencyValidation>& depVal = nullptr);
        void Enqueue(
            StringSection<char> shaderInMemory, StringSection<char> entryPoint, 
            StringSection<char> shaderModel, StringSection<ResChar> definesTable);

        ShaderCompileMarker(std::shared_ptr<ShaderService::ILowLevelCompiler>, std::shared_ptr<ISourceCodePreprocessor> preprocessor);
        ~ShaderCompileMarker();

        ShaderCompileMarker(ShaderCompileMarker&) = delete;
        ShaderCompileMarker& operator=(const ShaderCompileMarker&) = delete;
    protected:
        virtual void Complete(const void* buffer, size_t bufferSize);
		virtual void OnFailure();
        void CommitToArchive();

        Payload _payload;
        std::vector<::Assets::DependentFileState> _deps;
        ::Assets::rstring _definesTable;
        std::shared_ptr<ShaderService::ILowLevelCompiler> _compiler;
        std::shared_ptr<ISourceCodePreprocessor> _preprocessor;

        ChainFn _chain;
        ResId _shaderPath;
    };

    auto ShaderCompileMarker::GetDependencies() const 
        -> const std::vector<::Assets::DependentFileState>&
    {
        return _deps;
    }

    void ShaderCompileMarker::Enqueue(
        const ResId& shaderPath, const ::Assets::rstring& definesTable, 
        ChainFn chain,
        const std::shared_ptr<::Assets::DependencyValidation>& depVal)
    {
        _shaderPath = shaderPath;
        _definesTable = definesTable;
        _chain = std::move(chain);

        if (constant_expression<CompileInBackground>::result()) {

                // invoke a background load and compile...
                // note that Enqueue can't be called from a constructor, because it
                // calls shared_from_this()
            assert(0);
            /*
            AsyncLoadOperation::Enqueue(
				std::static_pointer_cast<ShaderCompileMarker>(shared_from_this()),
                _shaderPath._filename,
                ConsoleRig::GlobalServices::GetLongTaskThreadPool());
            */

        } else {

                // push file load & compile into this (foreground) thread
            if (_preprocessor) {
                auto preprocessedOutput = _preprocessor->RunPreprocessor(_shaderPath._filename);
                if (!preprocessedOutput._processedSource.empty()) {
                    static_assert(sizeof(decltype(preprocessedOutput._processedSource)::value_type) == 1, "Expecting single byte character for size calculation");
                    Complete(preprocessedOutput._processedSource.data(), preprocessedOutput._processedSource.size());
                } else {
                    OnFailure();
                }
            } else {
                size_t fileSize = 0;
                auto fileData = ::Assets::TryLoadFileAsMemoryBlock(_shaderPath._filename, &fileSize);
                if (fileData.get() && fileSize) {
                    Complete(fileData.get(), fileSize);
                } else {
                    OnFailure();
                }
            }

        }
    }

    void ShaderCompileMarker::Enqueue(
        StringSection<char> shaderInMemory, 
        StringSection<char> entryPoint, StringSection<char> shaderModel, StringSection<ResChar> definesTable)
    {
        _shaderPath = ResId(
            StringMeld<64>() << "ShaderInMemory_" << Hash64(shaderInMemory.begin(), shaderInMemory.end()), 
            entryPoint, shaderModel);
        _definesTable = definesTable.AsString();
        _chain = nullptr;

        if (constant_expression<CompileInBackground>::result()) {
            auto sharedToThis = std::static_pointer_cast<ShaderCompileMarker>(shared_from_this());
            std::string sourceCopy = shaderInMemory.AsString();
            ConsoleRig::GlobalServices::GetLongTaskThreadPool().Enqueue(
                [sourceCopy, sharedToThis]() { sharedToThis->Complete(AsPointer(sourceCopy.cbegin()), sourceCopy.size()); });
        } else {
            Complete(shaderInMemory.begin(), shaderInMemory.size());
        }
    }

    ShaderCompileMarker::ShaderCompileMarker(
        std::shared_ptr<ShaderService::ILowLevelCompiler> compiler,
        std::shared_ptr<ISourceCodePreprocessor> preprocessor)
    : _compiler(compiler), _preprocessor(preprocessor) {}
    ShaderCompileMarker::~ShaderCompileMarker() {}

    static bool CancelAllShaderCompiles = false;

	void ShaderCompileMarker::OnFailure()
	{
		_chain(_shaderPath, _definesTable, ::Assets::AssetState::Invalid, nullptr, nullptr, IteratorRange<const ::Assets::DependentFileState*>());
		SetState(::Assets::AssetState::Invalid);
	}

    void ShaderCompileMarker::Complete(const void* buffer, size_t bufferSize)
    {
        if (CancelAllShaderCompiles) {
			OnFailure();
            return;
        }

		TRY {
			Payload errors;
			_payload.reset();
			_deps.clear();

			auto success = _compiler->DoLowLevelCompile(
				_payload, errors, _deps,
				buffer, bufferSize, _shaderPath,
				_definesTable.c_str());

				// before we can finish the "complete" step, we need to commit
				// to archive output
			auto result = success ? ::Assets::AssetState::Ready : ::Assets::AssetState::Invalid;
        
				// We need to call "_chain" on either failure or success
				// this is important because _chain can hold a reference to this
				// object. We need to call chain explicitly in order to release
				// this object (otherwise we end up with a cyclic reference that
				// doesn't get broken, and a leak)
			if (_chain) {
				TRY {
					_chain(_shaderPath, _definesTable, result, _payload, errors, MakeIteratorRange(_deps));
				} CATCH (const std::bad_function_call& e) {
                    Log(Warning)
                        << "Chain function call failed in ShaderCompileMarker::Complete (with bad_function_call: " << e.what() << ")" << std::endl
                        << "This may prevent the shader from being flushed to disk in it's compiled form. But the shader should still be useable" << std::endl;
				} CATCH_END
			}

			SetState(result);
		} CATCH(...) {
			SetState(::Assets::AssetState::Invalid);
			throw;
		} CATCH_END
    }

    auto ShaderCompileMarker::Resolve(
        StringSection<::Assets::ResChar> initializer, 
        const std::shared_ptr<::Assets::DependencyValidation>& depVal) const -> const Payload&
    {
        auto state = GetAssetState();
        if (state == ::Assets::AssetState::Invalid)
            Throw(::Assets::Exceptions::InvalidAsset(initializer, "Invalid shader code while resolving"));

        if (state == ::Assets::AssetState::Pending) 
            Throw(::Assets::Exceptions::PendingAsset(initializer, "Pending shader code while resolving"));

        if (depVal)
            for (const auto& i:_deps)
                RegisterFileDependency(depVal, MakeStringSection(i._filename));

        return _payload;
    }

    auto ShaderCompileMarker::TryResolve(
        Payload& result,
        const std::shared_ptr<::Assets::DependencyValidation>& depVal) const -> ::Assets::AssetState
    {
        auto state = GetAssetState();
        if (state != ::Assets::AssetState::Ready)
            return state;

        if (depVal)
            for (const auto& i:_deps)
                RegisterFileDependency(depVal, MakeStringSection(i._filename));

        result = _payload;
        return ::Assets::AssetState::Ready;
    }

    auto ShaderCompileMarker::GetErrors() const -> Payload { return Payload(); }

    ::Assets::AssetState ShaderCompileMarker::StallWhilePending() const
    {
        return ::Assets::GenericFuture::StallWhilePending();
    }

        ////////////////////////////////////////////////////////////

    class ShaderCacheSet
    {
    public:
        std::shared_ptr<::Assets::ArchiveCache> GetArchive(
            const char shaderBaseFilename[], 
            const ::Assets::IntermediateAssets::Store& intermediateStore);

        void LogStats(const ::Assets::IntermediateAssets::Store& intermediateStore);

        ShaderCacheSet(const DeviceDesc& devDesc);
        ~ShaderCacheSet();
    protected:
        typedef std::pair<uint64, std::shared_ptr<::Assets::ArchiveCache>> Archive;
        std::vector<Archive>    _archives;
        Threading::Mutex        _archivesLock;
        std::string             _baseFolderName;
        std::string             _versionString;
        std::string             _buildDateString;
    };

    std::shared_ptr<::Assets::ArchiveCache> ShaderCacheSet::GetArchive(
        const char shaderBaseFilename[],
        const ::Assets::IntermediateAssets::Store& intermediateStore)
    {
        auto hashedName = Hash64(shaderBaseFilename);

        ScopedLock(_archivesLock);
        auto existing = LowerBound(_archives, hashedName);
        if (existing != _archives.cend() && existing->first == hashedName) {
            return existing->second;
        }

        char intName[MaxPath];
        XlCopyString(intName, _baseFolderName.c_str());
        XlCatString(intName, shaderBaseFilename);
        intermediateStore.MakeIntermediateName(intName, dimof(intName), intName);
        auto newArchive = std::make_shared<::Assets::ArchiveCache>(intName, _versionString.c_str(), _buildDateString.c_str());
        _archives.insert(existing, std::make_pair(hashedName, newArchive));
        return std::move(newArchive);
    }

    void ShaderCacheSet::LogStats(const ::Assets::IntermediateAssets::Store& intermediateStore)
    {
            // log statistics information for all shaders in all archive caches
        uint64 totalShaderSize = 0; // in bytes
        uint64 totalAllocationSpace = 0;

        char baseDir[MaxPath];
        intermediateStore.MakeIntermediateName(baseDir, dimof(baseDir), MakeStringSection(_baseFolderName));
        auto baseDirLen = XlStringLen(baseDir);
        assert(&baseDir[baseDirLen] == XlStringEnd(baseDir));
        std::deque<std::string> dirs;
        dirs.push_back(std::string(baseDir));

        std::vector<std::string> allArchives;
        while (!dirs.empty()) {
            auto dir = dirs.back();
            dirs.pop_back();

            auto files = RawFS::FindFiles(dir + "*.dir", RawFS::FindFilesFilter::File);
            allArchives.insert(allArchives.end(), files.begin(), files.end());

            auto subDirs = RawFS::FindFiles(dir + "*.*", RawFS::FindFilesFilter::Directory);
            for (auto d=subDirs.cbegin(); d!=subDirs.cend(); ++d) {
                if (!d->empty() && d->at(d->size()-1) != '.') {
                    dirs.push_back(*d + "/");
                }
            }
        }

            //  get metrics information about each archive and log it
            //  First, we'll have a "brief" log containing a list of all
            //  shader archives, and all shaders stored within them, with minimal
            //  information about each one.
            //  Then, we'll have a longer list with more profiling metrics about
            //  each shader.
        std::regex extractShaderDetails("\\[([^\\]]*)\\]\\s*\\[([^\\]]*)\\]\\s*\\[([^\\]]*)\\]");
        std::regex extractIntructionCount("Instruction Count:\\s*(\\d+)");

        Log(Verbose) << "------------------------------------------------------------------------------------------" << std::endl;
		Log(Verbose) << "    Shader cache readout" << std::endl;

        std::vector<std::pair<std::string, std::string>> extendedInfo;
        std::vector<std::pair<unsigned, std::string>> orderedByInstructionCount;
        for (auto i=allArchives.cbegin(); i!=allArchives.cend(); ++i) {
            char buffer[MaxPath];
            XlCopyString(buffer, i->c_str());

                // archive names should end in ".dir" at this point... we need to remove that .dir
                // we also have to remove the intermediate base dir from the front
            auto length = i->size();
            if (length >= 4 && buffer[length-4] == '.' && tolower(buffer[length-3]) == 'd' && tolower(buffer[length-2]) == 'i' && tolower(buffer[length-1]) == 'r') {
                buffer[length-4] = '\0';
            }
            if (!XlComparePrefixI(baseDir, buffer, baseDirLen)) {
                XlMoveMemory(buffer, &buffer[baseDirLen], length - baseDirLen + 1);
            }
            SplitPath<char>(buffer).Simplify().Rebuild(buffer, dimof(buffer));

            auto metrics = GetArchive(buffer, intermediateStore)->GetMetrics();
            totalShaderSize += metrics._usedSpace;
            totalAllocationSpace += metrics._allocatedFileSize;

                // write a short list of all shader objects stored in this archive
            float wasted = 0.f;
            if (totalAllocationSpace) { wasted = 1.f - (float(metrics._usedSpace) / float(metrics._allocatedFileSize)); }
			Log(Verbose) << " <<< Archive --- " << buffer << " (" << totalShaderSize / 1024 << "k, " << unsigned(100.f * wasted) << "% wasted) >>>" << std::endl;

            for (auto b = metrics._blocks.cbegin(); b!=metrics._blocks.cend(); ++b) {
                
                    //  attached string should be split into a number of section enclosed
                    //  in square brackets. The first and second sections contain 
                    //  shader file and entry point information, and the defines table.
                std::smatch match;
                bool a = std::regex_match(b->_attachedString, match, extractShaderDetails);
                if (a && match.size() >= 4) {
					Log(Verbose) << "    [" << b->_size/1024 << "k] [" << match[1] << "] [" << match[2] << "]" << std::endl;

                    auto idString = std::string("[") + match[1].str() + "][" + match[2].str() + "]";
                    extendedInfo.push_back(std::make_pair(idString, match[3]));

                    std::smatch intrMatch;
                    auto temp = match[3].str();
                    a = std::regex_search(temp, intrMatch, extractIntructionCount);
                    if (a && intrMatch.size() >= 1) {
                        auto instructionCount = XlAtoI32(intrMatch[1].str().c_str());
                        orderedByInstructionCount.push_back(std::make_pair(instructionCount, idString));
                    }
                } else {
					Log(Verbose) << "    [" << b->_size/1024 << "k] Unknown block" << std::endl;
                }
            }
        }

		Log(Verbose) << "------------------------------------------------------------------------------------------" << std::endl;
		Log(Verbose) << "    Ordered by instruction count" << std::endl;
        std::sort(orderedByInstructionCount.begin(), orderedByInstructionCount.end(), CompareFirst<unsigned, std::string>());
        for (auto e=orderedByInstructionCount.cbegin(); e!=orderedByInstructionCount.cend(); ++e) {
			Log(Verbose) << "    " << e->first << " " << e->second << std::endl;
        }

		Log(Verbose) << "------------------------------------------------------------------------------------------" << std::endl;
		Log(Verbose) << "    Shader cache extended info" << std::endl;
        for (auto e=extendedInfo.cbegin(); e!=extendedInfo.cend(); ++e) {
			Log(Verbose) << e->first << std::endl;
			Log(Verbose) << e->second << std::endl;
        }

		Log(Verbose) << "------------------------------------------------------------------------------------------" << std::endl;
		Log(Verbose) << "Total shader size: " << totalShaderSize << std::endl;
		Log(Verbose) << "Total allocated space: " << totalAllocationSpace << std::endl;
        if (totalAllocationSpace > 0) {
			Log(Verbose) << "Wasted part: " << 100.f * (1.0f - float(totalShaderSize) / float(totalAllocationSpace)) << "%" << std::endl;
        }
		Log(Verbose) << "------------------------------------------------------------------------------------------" << std::endl;
    }

    ShaderCacheSet::ShaderCacheSet(const DeviceDesc& devDesc)
    {
        _baseFolderName = std::string(devDesc._underlyingAPI) + "/";
        _versionString = devDesc._buildVersion;
        _buildDateString = devDesc._buildDate;
    }

    ShaderCacheSet::~ShaderCacheSet() {}

        ////////////////////////////////////////////////////////////

	using Blob = ::Assets::Blob;

	class ArchivedFileArtifact : public ::Assets::IArtifact
	{
	public:
		Blob	GetBlob() const;
		Blob	GetErrors() const;
		::Assets::DepValPtr GetDependencyValidation() const;
		ArchivedFileArtifact(
			const std::shared_ptr<::Assets::ArchiveCache>& archive, uint64 fileID, const ::Assets::DepValPtr& depVal,
			const Blob& blob, const Blob& errors);
		~ArchivedFileArtifact();
	private:
		std::shared_ptr<::Assets::ArchiveCache> _archive;
		uint64 _fileID;
		::Assets::DepValPtr _depVal;
		Blob _blob, _errors;
	};

	auto ArchivedFileArtifact::GetBlob() const -> Blob
	{
		if (_blob) return _blob;
		return _archive->TryOpenFromCache(_fileID);
	}

	auto ArchivedFileArtifact::GetErrors() const -> Blob { return _errors; }
	::Assets::DepValPtr ArchivedFileArtifact::GetDependencyValidation() const { return _depVal; }

	ArchivedFileArtifact::ArchivedFileArtifact(
		const std::shared_ptr<::Assets::ArchiveCache>& archive, uint64 fileID, const ::Assets::DepValPtr& depVal,
		const Blob& blob, const Blob& errors)
	: _archive(archive), _fileID(fileID), _depVal(depVal)
	, _blob(blob), _errors(errors) {}
	ArchivedFileArtifact::~ArchivedFileArtifact() {}

        ////////////////////////////////////////////////////////////

    class LocalCompiledShaderSource::Marker : public ::Assets::ICompileMarker
    {
    public:
        std::shared_ptr<::Assets::IArtifact> GetExistingAsset() const;
        std::shared_ptr<::Assets::CompileFuture> InvokeCompile() const;
        StringSection<::Assets::ResChar> Initializer() const;

        Marker(
            StringSection<::Assets::ResChar> initializer, 
            const ShaderService::ResId& res, StringSection<::Assets::ResChar> definesTable,
            const ::Assets::IntermediateAssets::Store& store,
            std::shared_ptr<LocalCompiledShaderSource> compiler);
        ~Marker();
    protected:
        ShaderService::ResId _res;
        ::Assets::rstring _definesTable;
        ::Assets::rstring _initializer;
        std::weak_ptr<LocalCompiledShaderSource> _compiler;
        const ::Assets::IntermediateAssets::Store* _store;
    };

    static uint64 GetTarget(
		const ShaderService::ResId& res, const ::Assets::rstring& definesTable,
        /*out*/ ::Assets::ResChar archiveName[], size_t archiveNameCount,
        /*out*/ ::Assets::ResChar depName[], size_t depNameCount)
    {
        snprintf(archiveName, archiveNameCount * sizeof(::Assets::ResChar), "%s-%s", res._filename, res._shaderModel);
        auto archiveId = HashCombine(Hash64(res._entryPoint), Hash64(definesTable));
        snprintf(depName, depNameCount * sizeof(::Assets::ResChar), "%s-%08x%08x", archiveName, uint32(archiveId>>32ull), uint32(archiveId));
		return archiveId;
    }

    std::shared_ptr<::Assets::IArtifact> LocalCompiledShaderSource::Marker::GetExistingAsset() const
    {
        auto c = _compiler.lock();
        if (!c || CancelAllShaderCompiles) return nullptr;

        ::Assets::ResChar archiveName[MaxPath], depName[MaxPath];
        auto archiveId = GetTarget(_res, _definesTable, archiveName, dimof(archiveName), depName, dimof(depName));

        return std::make_shared<ArchivedFileArtifact>(
			c->_shaderCacheSet->GetArchive(archiveName, *_store), archiveId, 
			_store->MakeDependencyValidation(depName),
            nullptr, nullptr);
    }

	void LocalCompiledShaderSource::AddCompileOperation(const std::shared_ptr<ShaderCompileMarker>& marker)
	{
		Interlocked::Increment(&_activeCompileCount);
        {
                // unfortunately we need to lock this... because we search through it in
                // a background thread
            ScopedLock(_activeCompileOperationsLock);
            _activeCompileOperations.push_back(marker);
        }
	}

	void LocalCompiledShaderSource::RemoveCompileOperation(ShaderCompileMarker& marker)
	{
        ScopedLock(_activeCompileOperationsLock);
        auto i = std::find_if(
            _activeCompileOperations.begin(), _activeCompileOperations.end(), 
            [&marker](std::shared_ptr<ShaderCompileMarker>& test) { return test.get() == &marker; });
        if (i != _activeCompileOperations.end()) {
            _activeCompileOperations.erase(i);
            Interlocked::Decrement(&_activeCompileCount);
        }
    }

	static ::Assets::DepValPtr AsDepValPtr(IteratorRange<const ::Assets::DependentFileState*> deps)
	{
		auto result = std::make_shared<::Assets::DependencyValidation>();
		for (const auto& i:deps)
			::Assets::RegisterFileDependency(result, MakeStringSection(i._filename));
		return result;
	}

    std::shared_ptr<::Assets::CompileFuture> LocalCompiledShaderSource::Marker::InvokeCompile() const
    {
        auto c = _compiler.lock();
        if (!c || CancelAllShaderCompiles) return nullptr;

        auto marker = std::make_shared<::Assets::CompileFuture>();

        #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
                //  When we have archive attachments enabled, we can write
                //  some information to help identify this shader object
                //  We'll start with something to define the object...
            std::stringstream builder;
            builder 
                << "[" << _res._filename
                << ":" << _res._entryPoint
                << ":" << _res._shaderModel
                << "] [" << _definesTable << "]";
            auto archiveCacheAttachment = builder.str();
        #else
            int archiveCacheAttachment;
        #endif

        using Payload = ShaderCompileMarker::Payload;

        auto compileHelper = std::make_shared<ShaderCompileMarker>(c->_compiler, c->_preprocessor);

        auto tempPtr = compileHelper.get();
        auto store = _store;
        compileHelper->Enqueue(
            _res, _definesTable,
            [marker, archiveCacheAttachment, store, tempPtr, c]
            (   const ShaderService::ResId& resId, const std::string& definesTable,
				::Assets::AssetState newState, const Payload& payload, const Payload& errors,
                IteratorRange<const ::Assets::DependentFileState*> deps)
            {
				::Assets::ResChar archiveName[MaxPath], depName[MaxPath];
				auto archiveId = GetTarget(
					resId, definesTable,
					archiveName, dimof(archiveName), 
					depName, dimof(depName));
				auto archive = c->_shaderCacheSet->GetArchive(archiveName, *store);

                if (newState == ::Assets::AssetState::Ready && archive) {
                    assert(payload.get() && payload->size() > 0);

                    #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
                        auto metricsString = 
                            c->_compiler->MakeShaderMetricsString(
                                AsPointer(payload->cbegin()), payload->size());
                    #endif

                    std::vector<::Assets::DependentFileState> depsAsVector(deps.begin(), deps.end());
                    auto baseDirAsString = MakeFileNameSplitter(resId._filename).DriveAndPath().AsString();
					auto depNameAsString = std::string(depName);

                    archive->Commit(
                        archiveId, Payload(payload),
                        #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
                            (archiveCacheAttachment + " [" + metricsString + "]"),
                        #else
                            std::string(),
                        #endif

                                // on flush, we need to write out the dependencies file
                                // note that delaying the call to WriteDependencies requires
                                // many small annoying allocations! It's much simplier if we
                                // can just write them now -- but it causes problems if we a
                                // crash or use End Debugging before we flush the archive
                        [depsAsVector, depNameAsString, baseDirAsString, store]()
                        { store->WriteDependencies(
                            depNameAsString.c_str(), StringSection<::Assets::ResChar>(baseDirAsString), 
                            MakeIteratorRange(depsAsVector), false); });
                    (void)archiveCacheAttachment;
                }

					// Create the artifact and add it to the compile marker
				auto depVal = AsDepValPtr(deps);
				auto artifact = std::make_shared<::Assets::BlobArtifact>(payload, errors, depVal);
				marker->AddArtifact("main", artifact);

                    // give the CompileFuture object the same state
                marker->SetState(newState);

                c->RemoveCompileOperation(*tempPtr);
            });

        return std::move(marker);
    }

    StringSection<::Assets::ResChar> LocalCompiledShaderSource::Marker::Initializer() const
    {
        return MakeStringSection(_initializer);
    }

    LocalCompiledShaderSource::Marker::Marker(
        StringSection<::Assets::ResChar> initializer, const ShaderService::ResId& res, StringSection<::Assets::ResChar> definesTable,
        const ::Assets::IntermediateAssets::Store& store,
        std::shared_ptr<LocalCompiledShaderSource> compiler)
    : _initializer(initializer.AsString()), _res(res), _definesTable(definesTable.AsString()), _compiler(std::move(compiler)), _store(&store)
    {}

    LocalCompiledShaderSource::Marker::~Marker() {}
    
    std::shared_ptr<::Assets::ICompileMarker> LocalCompiledShaderSource::PrepareAsset(
        uint64 typeCode, const StringSection<ResChar> initializers[], unsigned initializerCount,
        const ::Assets::IntermediateAssets::Store& destinationStore)
    {
            //  Execute an offline compile. This should happen in the background
            //  When it's complete, we'll write the result into the appropriate
            //  archive file and trigger a message to the caller.
            //
            //      To compiler, we need only a few bits of information:
            //          main shader file
            //          entry point function
            //          shader model (including shader stage and shader model version)
            //          defines table.
            //
            //  We want to combine many compiled shaders into a single file on this. We'll do this
            //  based on the main shader file & entry point. It's convenient because the dependencies
            //  should be largely the same for every compiled shader in that archive.
            //
            //  Though, there are cases where a #if might skip over an #include, and thereby create 
            //  different dependencies. We'll ignore that, though, and just assume there is a single
            //  set of dependencies for every version of the shader that comes from the same main
            //  shader file.
            //
            //  We could have separate files for the defines table, as well. However, this might be
            //  inconvenient, because it could mean thousands of very small files. Also, the filenames
            //  of the completed assets would grow very large (because the defines tables can be quite
            //  long). So, let's avoid that by caching many compiled shaders together in archive files.
            //  We just have to be careful about multiple threads or multiple processes accessing the
            //  same file at the same time (particularly if one is doing a read, and another is doing
            //  a write that changes the offsets within the file).

        if (CancelAllShaderCompiles) {
            return nullptr; // can't start a new compile now. Probably we're shutting down
        }

        auto shaderId = ShaderService::MakeResId(initializers[0], _compiler.get());
		StringSection<ResChar> definesTable = (initializerCount > 1)?initializers[1]:StringSection<ResChar>();

            // for a "null" shader, we must return nullptr
        if (initializers[0].IsEmpty() || XlEqString(shaderId._filename, "null"))
            return nullptr;

        return std::make_shared<Marker>(initializers[0], shaderId, definesTable, destinationStore, shared_from_this());
    }

    auto LocalCompiledShaderSource::CompileFromFile(
        StringSection<ResChar> resource, 
        StringSection<ResChar> definesTable) const -> std::shared_ptr<::Assets::CompileFuture>
    {
        auto compileHelper = std::make_shared<ShaderCompileMarker>(_compiler, _preprocessor);
        auto resId = ShaderService::MakeResId(resource, _compiler.get());
        compileHelper->Enqueue(resId, definesTable.AsString(), nullptr);
        return compileHelper;
    }
            
    auto LocalCompiledShaderSource::CompileFromMemory(
        StringSection<char> shaderInMemory, StringSection<char> entryPoint, 
        StringSection<char> shaderModel, StringSection<ResChar> definesTable) const -> std::shared_ptr<::Assets::CompileFuture>
    {
        auto compileHelper = std::make_shared<ShaderCompileMarker>(_compiler, _preprocessor);
        compileHelper->Enqueue(shaderInMemory, entryPoint, shaderModel, definesTable); 
        return compileHelper;
    }

    void LocalCompiledShaderSource::StallOnPendingOperations(bool cancelAll)
    {
        if (cancelAll) CancelAllShaderCompiles = true;

            // Stall until all pending operations have finished.
            // We don't have a safe way to cancel active operations...
            //  though perhaps we could prevent new operations from starting
        while (Interlocked::Load(&_activeCompileCount) != 0) {
            Threading::Pause();
        }
    }

    LocalCompiledShaderSource::LocalCompiledShaderSource(
        std::shared_ptr<ShaderService::ILowLevelCompiler> compiler,
        std::shared_ptr<ISourceCodePreprocessor> preprocessor,
        const DeviceDesc& devDesc)
    : _compiler(std::move(compiler))
    , _preprocessor(std::move(preprocessor))
    {
        CancelAllShaderCompiles = false;
        _shaderCacheSet = std::make_unique<ShaderCacheSet>(devDesc);
        Interlocked::Exchange(&_activeCompileCount, 0);
    }

    LocalCompiledShaderSource::~LocalCompiledShaderSource()
    {
        if (Interlocked::Load(&_activeCompileCount) != 0) {
            #if defined(XLE_HAS_CONSOLE_RIG)
                LogWarning << "Shader compile operations still pending while attempt to shutdown LocalCompiledShaderSource! Stalling until finished";
            #endif
            StallOnPendingOperations(true);
        }
    }

}}

