// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LocalCompiledShaderSource.h"
#include "../Metal/Shader.h"

#include "../../Assets/ChunkFile.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/ArchiveCache.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/AsyncLoadOperation.h"

#include "../../../ConsoleRig/Log.h"
#include "../../../ConsoleRig/GlobalServices.h"
#include "../../../Utility/Streams/PathUtils.h"
#include "../../../Utility/Streams/FileUtils.h"
#include "../../../Utility/Threading/CompletionThreadPool.h"
#include "../../../Utility/StringFormat.h"

#include <functional>
#include <deque>
#include <regex>

namespace RenderCore 
{ 
    extern char VersionString[];
    extern char BuildDateString[];
}

namespace RenderCore { namespace Assets 
{
    static const bool CompileInBackground = true;
    using ::Assets::ResChar;
    using ResId = ShaderService::ResId;

        ////////////////////////////////////////////////////////////

    class ShaderCompileMarker : public ShaderService::IPendingMarker, public ::Assets::AsyncLoadOperation
    {
    public:
        using Payload = std::shared_ptr<std::vector<uint8>>;
        using ChainFn = std::function<void(
            ::Assets::AssetState, const Payload& payload,
            const ::Assets::DependentFileState*, const ::Assets::DependentFileState*)>;

        const Payload& Resolve(const char initializer[], const std::shared_ptr<::Assets::DependencyValidation>& depVal = nullptr) const;

        ::Assets::AssetState TryResolve(
            Payload& result,
            const std::shared_ptr<::Assets::DependencyValidation>& depVal) const;
        Payload GetErrors() const;

        ::Assets::AssetState StallWhilePending() const;

        const std::vector<::Assets::DependentFileState>& GetDependencies() const;

        void Enqueue(
            const ResId& shaderPath, ::Assets::rstring definesTable, 
            ChainFn chain = nullptr,
            const std::shared_ptr<::Assets::DependencyValidation>& depVal = nullptr);
        void Enqueue(
            const char shaderInMemory[], const char entryPoint[], 
            const char shaderModel[], const ResChar definesTable[]);

        ShaderStage::Enum GetStage() const { return _shaderPath.AsShaderStage(); }

        ShaderCompileMarker(std::shared_ptr<ShaderService::ILowLevelCompiler>);
        ~ShaderCompileMarker();

        ShaderCompileMarker(ShaderCompileMarker&) = delete;
        ShaderCompileMarker& operator=(const ShaderCompileMarker&) = delete;
    protected:
        virtual ::Assets::AssetState Complete(const void* buffer, size_t bufferSize);
        void CommitToArchive();

        Payload _payload;
        std::vector<::Assets::DependentFileState> _deps;
        ::Assets::rstring _definesTable;
        std::shared_ptr<ShaderService::ILowLevelCompiler> _compiler;

        ChainFn _chain;
        ResId _shaderPath;
    };

    auto ShaderCompileMarker::GetDependencies() const 
        -> const std::vector<::Assets::DependentFileState>&
    {
        return _deps;
    }

    void ShaderCompileMarker::Enqueue(
        const ResId& shaderPath, ::Assets::rstring definesTable, 
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
            AsyncLoadOperation::Enqueue(
                _shaderPath._filename,
                ConsoleRig::GlobalServices::GetLongTaskThreadPool());

        } else {

                // push file load & compile into this (foreground) thread
            size_t fileSize = 0;
            auto fileData = LoadFileAsMemoryBlock(_shaderPath._filename, &fileSize);
            ::Assets::AssetState state = ::Assets::AssetState::Invalid;

            if (fileData.get() && fileSize)
                state = Complete(fileData.get(), fileSize);

            SetState(state);

        }
    }

    void ShaderCompileMarker::Enqueue(
        const char shaderInMemory[], 
        const char entryPoint[], const char shaderModel[], const ResChar definesTable[])
    {
        size_t shaderBufferSize = XlStringLen(shaderInMemory);
        assert(&shaderInMemory[shaderBufferSize] == XlStringEnd(shaderInMemory));

        _shaderPath = ResId(
            StringMeld<64>() << "ShaderInMemory_" << Hash64(shaderInMemory, &shaderInMemory[shaderBufferSize]), 
            entryPoint, shaderModel);
        if (definesTable) _definesTable = definesTable;
        _chain = nullptr;

        if (constant_expression<CompileInBackground>::result()) {

            auto sharedToThis = shared_from_this();
            std::string sourceCopy(shaderInMemory, &shaderInMemory[shaderBufferSize]);
            ConsoleRig::GlobalServices::GetLongTaskThreadPool().Enqueue(
                [sourceCopy, sharedToThis, this]()
                {
                    auto state = this->Complete(AsPointer(sourceCopy.cbegin()), sourceCopy.size());
                    sharedToThis->SetState(state);
                });

        } else {
            auto state = Complete(shaderInMemory, shaderBufferSize);
            SetState(state);
        }
    }

    ShaderCompileMarker::ShaderCompileMarker(std::shared_ptr<ShaderService::ILowLevelCompiler> compiler)
    : _compiler(compiler) {}
    ShaderCompileMarker::~ShaderCompileMarker() {}

    static bool CancelAllShaderCompiles = false;

    ::Assets::AssetState ShaderCompileMarker::Complete(
        const void* buffer, size_t bufferSize)
    {
        if (CancelAllShaderCompiles) {
            _chain(::Assets::AssetState::Invalid, nullptr, nullptr, nullptr);
            return ::Assets::AssetState::Invalid;
        }

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
            TRY 
            {
                _chain(
                    result, _payload, 
                    AsPointer(_deps.cbegin()), AsPointer(_deps.cend()));
            } CATCH (const std::bad_function_call& e) {
                LogWarning 
                    << "Chain function call failed in ShaderCompileMarker::Complete (with bad_function_call: " << e.what() << ")" // << std::endl 
                    << "This may prevent the shader from being flushed to disk in it's compiled form. But the shader should still be useable";
            } CATCH_END
        }
        return result;
    }

    auto ShaderCompileMarker::Resolve(
        const char initializer[], 
        const std::shared_ptr<::Assets::DependencyValidation>& depVal) const -> const Payload&
    {
        auto state = GetState();
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
        auto state = GetState();
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
        return ::Assets::PendingOperationMarker::StallWhilePending();
    }

        ////////////////////////////////////////////////////////////

    class ShaderCacheSet
    {
    public:
        std::shared_ptr<::Assets::ArchiveCache> GetArchive(
            const char shaderBaseFilename[], 
            const ::Assets::IntermediateAssets::Store& intermediateStore);

        void LogStats(const ::Assets::IntermediateAssets::Store& intermediateStore);

        ShaderCacheSet();
        ~ShaderCacheSet();
    protected:
        typedef std::pair<uint64, std::shared_ptr<::Assets::ArchiveCache>> Archive;
        std::vector<Archive> _archives;
        Threading::Mutex _archivesLock;
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
        intermediateStore.MakeIntermediateName(intName, dimof(intName), shaderBaseFilename);
        auto newArchive = std::make_shared<::Assets::ArchiveCache>(intName, VersionString, BuildDateString);
        _archives.insert(existing, std::make_pair(hashedName, newArchive));
        return std::move(newArchive);
    }

    void ShaderCacheSet::LogStats(const ::Assets::IntermediateAssets::Store& intermediateStore)
    {
            // log statistics information for all shaders in all archive caches
        uint64 totalShaderSize = 0; // in bytes
        uint64 totalAllocationSpace = 0;

        char baseDir[MaxPath];
        intermediateStore.MakeIntermediateName(baseDir, dimof(baseDir), "");
        auto baseDirLen = XlStringLen(baseDir);
        assert(&baseDir[baseDirLen] == XlStringEnd(baseDir));
        std::deque<std::string> dirs;
        dirs.push_back(std::string(baseDir));

        std::vector<std::string> allArchives;
        while (!dirs.empty()) {
            auto dir = dirs.back();
            dirs.pop_back();

            auto files = FindFiles(dir + "*.dir", FindFilesFilter::File);
            allArchives.insert(allArchives.end(), files.begin(), files.end());

            auto subDirs = FindFiles(dir + "*.*", FindFilesFilter::Directory);
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

        LogInfo << "------------------------------------------------------------------------------------------";
        LogInfo << "    Shader cache readout";

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
            LogInfo << " <<< Archive --- " << buffer << " (" << totalShaderSize / 1024 << "k, " << unsigned(100.f * wasted) << "% wasted) >>>";

            for (auto b = metrics._blocks.cbegin(); b!=metrics._blocks.cend(); ++b) {
                
                    //  attached string should be split into a number of section enclosed
                    //  in square brackets. The first and second sections contain 
                    //  shader file and entry point information, and the defines table.
                std::smatch match;
                bool a = std::regex_match(b->_attachedString, match, extractShaderDetails);
                if (a && match.size() >= 4) {
                    LogInfo << "    [" << b->_size/1024 << "k] [" << match[1] << "] [" << match[2] << "]";

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
                    LogInfo << "    [" << b->_size/1024 << "k] Unknown block";
                }
            }
        }

        LogInfo << "------------------------------------------------------------------------------------------";
        LogInfo << "    Ordered by instruction count";
        std::sort(orderedByInstructionCount.begin(), orderedByInstructionCount.end(), CompareFirst<unsigned, std::string>());
        for (auto e=orderedByInstructionCount.cbegin(); e!=orderedByInstructionCount.cend(); ++e) {
            LogInfo << "    " << e->first << " " << e->second;
        }

        LogInfo << "------------------------------------------------------------------------------------------";
        LogInfo << "    Shader cache extended info";
        for (auto e=extendedInfo.cbegin(); e!=extendedInfo.cend(); ++e) {
            LogInfo << e->first;
            LogInfo << e->second;
        }

        LogInfo << "------------------------------------------------------------------------------------------";
        LogInfo << "Total shader size: " << totalShaderSize;
        LogInfo << "Total allocated space: " << totalAllocationSpace;
        if (totalAllocationSpace > 0) {
            LogInfo << "Wasted part: " << 100.f * (1.0f - float(totalShaderSize) / float(totalAllocationSpace)) << "%";
        }
        LogInfo << "------------------------------------------------------------------------------------------";
    }

    ShaderCacheSet::ShaderCacheSet() {}
    ShaderCacheSet::~ShaderCacheSet() {}

        ////////////////////////////////////////////////////////////

    class LocalCompiledShaderSource::Marker : public ::Assets::ICompileMarker
    {
    public:
        ::Assets::IntermediateAssetLocator GetExistingAsset() const;
        std::shared_ptr<::Assets::PendingCompileMarker> InvokeCompile() const;
        StringSection<::Assets::ResChar> Initializer() const;

        Marker(
            const ::Assets::ResChar initializer[], 
            const ShaderService::ResId& res, const ::Assets::ResChar definesTable[],
            const ::Assets::IntermediateAssets::Store& store,
            std::shared_ptr<LocalCompiledShaderSource> compiler);
        ~Marker();
    protected:
        ShaderService::ResId _res;
        ::Assets::rstring _definesTable;
        ::Assets::rstring _initializer;
        std::weak_ptr<LocalCompiledShaderSource> _compiler;
        const ::Assets::IntermediateAssets::Store* _store;

        void GetTarget(
            ::Assets::ResChar archiveName[], size_t archiveNameCount,
            ::Assets::ResChar depName[], size_t depNameCount,
            uint64& archiveId) const;
    };

    void LocalCompiledShaderSource::Marker::GetTarget(
        ::Assets::ResChar archiveName[], size_t archiveNameCount,
        ::Assets::ResChar depName[], size_t depNameCount,
        uint64& archiveId) const
    {
        _snprintf_s(archiveName, archiveNameCount * sizeof(::Assets::ResChar), _TRUNCATE, "%s-%s", _res._filename, _res._shaderModel);
        archiveId = HashCombine(Hash64(_res._entryPoint), Hash64(_definesTable));
        _snprintf_s(depName, depNameCount * sizeof(::Assets::ResChar), _TRUNCATE, "%s-%08x%08x", archiveName, uint32(archiveId>>32ull), uint32(archiveId));
    }

    ::Assets::IntermediateAssetLocator LocalCompiledShaderSource::Marker::GetExistingAsset() const
    {
        auto c = _compiler.lock();
        if (!c || CancelAllShaderCompiles) return ::Assets::IntermediateAssetLocator();

        ::Assets::ResChar archiveName[MaxPath], depName[MaxPath];
        uint64 archiveId;
        GetTarget(archiveName, dimof(archiveName), depName, dimof(depName), archiveId);

        ::Assets::IntermediateAssetLocator result;
        result._dependencyValidation = _store->MakeDependencyValidation(depName);
        XlCopyString(result._sourceID0, archiveName);
        result._sourceID1 = archiveId;
        result._archive = c->_shaderCacheSet->GetArchive(archiveName, *_store);
        return std::move(result);
    }

    std::shared_ptr<::Assets::PendingCompileMarker> LocalCompiledShaderSource::Marker::InvokeCompile() const
    {
        auto c = _compiler.lock();
        if (!c || CancelAllShaderCompiles) return nullptr;

        auto marker = std::make_shared<::Assets::PendingCompileMarker>();

        ::Assets::ResChar archiveName[MaxPath], depName[MaxPath];
        GetTarget(
            archiveName, dimof(archiveName), 
            depName, dimof(depName), marker->GetLocator()._sourceID1);
        marker->GetLocator()._archive = c->_shaderCacheSet->GetArchive(archiveName, *_store);

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

        ::Assets::rstring depNameAsString = depName;
        auto compileHelper = std::make_shared<ShaderCompileMarker>(c->_compiler);

        Interlocked::Increment(&c->_activeCompileCount);
        {
                // unfortunately we need to lock this... because we search through it in
                // a background thread
            ScopedLock(c->_activeCompileOperationsLock);
            c->_activeCompileOperations.push_back(compileHelper);
        }

        auto tempPtr = compileHelper.get();
        auto store = _store;
        compileHelper->Enqueue(
            _res, _definesTable,
            [marker, archiveCacheAttachment, depNameAsString, store, tempPtr, c]
            (   ::Assets::AssetState newState, const Payload& payload, 
                const ::Assets::DependentFileState* depsBegin, const ::Assets::DependentFileState* depsEnd)
            {
                if (newState == ::Assets::AssetState::Ready && marker->GetLocator()._archive) {
                    assert(payload.get() && payload->size() > 0);

                    #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
                        auto metricsString = 
                            c->_compiler->MakeShaderMetricsString(
                                AsPointer(payload->cbegin()), payload->size());
                    #endif

                    std::vector<::Assets::DependentFileState> deps(depsBegin, depsEnd);
                    auto baseDirAsString = MakeFileNameSplitter(marker->GetLocator()._sourceID0).DriveAndPath().AsString();

                    marker->GetLocator()._archive->Commit(
                        marker->GetLocator()._sourceID1, Payload(payload),
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
                        [deps, depNameAsString, baseDirAsString, store]()
                        { store->WriteDependencies(
                            depNameAsString.c_str(), StringSection<::Assets::ResChar>(baseDirAsString), 
                            MakeIteratorRange(deps), false); });
                    (void)archiveCacheAttachment;
                }

                marker->GetLocator()._dependencyValidation = std::make_shared<::Assets::DependencyValidation>();
                for (auto i=depsBegin; i!=depsEnd; ++i)
                    RegisterFileDependency(marker->GetLocator()._dependencyValidation, i->_filename.c_str());

                    // give the PendingCompileMarker object the same state
                marker->SetState(newState);

                {
                    ScopedLock(c->_activeCompileOperationsLock);
                    auto i = std::find_if(
                        c->_activeCompileOperations.begin(), c->_activeCompileOperations.end(), 
                        [tempPtr](std::shared_ptr<ShaderCompileMarker>& test) { return test.get() == tempPtr; });
                    if (i != c->_activeCompileOperations.end()) {
                        c->_activeCompileOperations.erase(i);
                        Interlocked::Decrement(&c->_activeCompileCount);
                    }
                }
            });

        return std::move(marker);
    }

    StringSection<::Assets::ResChar> LocalCompiledShaderSource::Marker::Initializer() const
    {
        return MakeStringSection(_initializer);
    }

    LocalCompiledShaderSource::Marker::Marker(
        const ::Assets::ResChar initializer[], const ShaderService::ResId& res, const ::Assets::ResChar definesTable[],
        const ::Assets::IntermediateAssets::Store& store,
        std::shared_ptr<LocalCompiledShaderSource> compiler)
    : _initializer(initializer), _res(res), _definesTable(definesTable), _compiler(std::move(compiler)), _store(&store)
    {}

    LocalCompiledShaderSource::Marker::~Marker() {}
    
    std::shared_ptr<::Assets::ICompileMarker> LocalCompiledShaderSource::PrepareAsset(
        uint64 typeCode, const ResChar* initializers[], unsigned initializerCount,
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

        auto shaderId = ShaderService::MakeResId(initializers[0], *_compiler);
        const char* definesTable = (initializerCount > 1)?initializers[1]:"";

            // for a "null" shader, we must return nullptr
        if (!initializers[0] || initializers[0][0] == '\0' || XlEqString(shaderId._filename, "null"))
            return nullptr;

        return std::make_shared<Marker>(initializers[0], shaderId, definesTable, destinationStore, shared_from_this());
    }

    auto LocalCompiledShaderSource::CompileFromFile(
        const ::Assets::ResChar resource[], 
        const ResChar definesTable[]) const -> std::shared_ptr<IPendingMarker>
    {
        auto compileHelper = std::make_shared<ShaderCompileMarker>(_compiler);
        auto resId = ShaderService::MakeResId(resource, *_compiler);
        compileHelper->Enqueue(resId, definesTable?definesTable:"", nullptr);
        return compileHelper;
    }
            
    auto LocalCompiledShaderSource::CompileFromMemory(
        const char shaderInMemory[], const char entryPoint[], 
        const char shaderModel[], const ResChar definesTable[]) const -> std::shared_ptr<IPendingMarker>
    {
        auto compileHelper = std::make_shared<ShaderCompileMarker>(_compiler);
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

    LocalCompiledShaderSource::LocalCompiledShaderSource(std::shared_ptr<ShaderService::ILowLevelCompiler> compiler)
    : _compiler(std::move(compiler))
    {
        CancelAllShaderCompiles = false;
        _shaderCacheSet = std::make_unique<ShaderCacheSet>();
        Interlocked::Exchange(&_activeCompileCount, 0);
    }

    LocalCompiledShaderSource::~LocalCompiledShaderSource()
    {
        if (Interlocked::Load(&_activeCompileCount) != 0) {
            LogWarning << "Shader compile operations still pending while attempt to shutdown LocalCompiledShaderSource! Stalling until finished";
            StallOnPendingOperations(true);
        }
    }

}}

