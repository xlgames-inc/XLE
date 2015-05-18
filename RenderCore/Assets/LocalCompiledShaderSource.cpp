// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LocalCompiledShaderSource.h"
#include "../Metal/Shader.h"

#include "../../Assets/ChunkFile.h"
#include "../../Assets/IntermediateResources.h"
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
    using ResId = Metal::ShaderService::ResId;

        ////////////////////////////////////////////////////////////

    class CompileHelper : public Metal::ShaderService::IPendingMarker, public ::Assets::AsyncLoadOperation
    {
    public:
        using Payload = std::shared_ptr<std::vector<uint8>>;
        using ChainFn = std::function<void(
            ::Assets::AssetState, const Payload& payload,
            const ::Assets::DependentFileState*, const ::Assets::DependentFileState*)>;

        const Payload& Resolve(
            const char initializer[],
            const std::shared_ptr<::Assets::DependencyValidation>& depVal = nullptr) const;

        const std::vector<::Assets::DependentFileState>& GetDependencies() const;

        void Enqueue(
            const ResId& shaderPath, const ResChar definesTable[], 
            ChainFn chain = nullptr,
            const std::shared_ptr<::Assets::DependencyValidation>& depVal = nullptr);
        void Enqueue(
            const char shaderInMemory[], const char entryPoint[], 
            const char shaderModel[], const ResChar definesTable[]);

        CompileHelper();
        ~CompileHelper();

        CompileHelper(CompileHelper&) = delete;
        CompileHelper& operator=(const CompileHelper&) = delete;
    protected:
        virtual ::Assets::AssetState Complete(const void* buffer, size_t bufferSize);
        void CommitToArchive();

        Payload _payload;
        std::vector<::Assets::DependentFileState> _deps;
        ::Assets::rstring _definesTable;

        ChainFn _chain;
        ResId _shaderPath;
    };

    auto CompileHelper::GetDependencies() const 
        -> const std::vector<::Assets::DependentFileState>&
    {
        return _deps;
    }

    void CompileHelper::Enqueue(
        const ResId& shaderPath, const ResChar definesTable[], 
        ChainFn chain,
        const std::shared_ptr<::Assets::DependencyValidation>& depVal)
    {
        _shaderPath = shaderPath;
        if (definesTable) _definesTable = definesTable;
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

    void CompileHelper::Enqueue(
        const char shaderInMemory[], 
        const char entryPoint[], const char shaderModel[], const ResChar definesTable[])
    {
        size_t shaderBufferSize = XlStringLen(shaderInMemory);

        _shaderPath = ResId(
            StringMeld<64>() << "ShaderInMemory_" << Hash64(shaderInMemory, &shaderInMemory[shaderBufferSize]), 
            entryPoint, shaderModel);;
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

    CompileHelper::CompileHelper() {}
    CompileHelper::~CompileHelper() {}

    ::Assets::AssetState CompileHelper::Complete(
        const void* buffer, size_t bufferSize)
    {
        Payload errors;
        _payload.reset();
        _deps.clear();

        auto& compiler = Metal::ShaderService::GetInstance().GetLowLevelCompiler();
        auto success = compiler.DoLowLevelCompile(
            _payload, errors, _deps,
            buffer, bufferSize, _shaderPath,
            _definesTable);

            // before we can finish the "complete" step, we need to commit
            // to archive output
        if (success) {
            _chain(
                ::Assets::AssetState::Ready, _payload, 
                AsPointer(_deps.cbegin()), AsPointer(_deps.cend()));
            return ::Assets::AssetState::Ready;
        } else {
            return ::Assets::AssetState::Invalid;
        }
    }

    auto CompileHelper::Resolve(
        const char initializer[], 
        const std::shared_ptr<::Assets::DependencyValidation>& depVal) const -> const Payload&
    {
        auto state = GetState();
        if (state == ::Assets::AssetState::Invalid)
            ThrowException(::Assets::Exceptions::InvalidResource(initializer, ""));

        if (state == ::Assets::AssetState::Pending) 
            ThrowException(::Assets::Exceptions::PendingResource(initializer, ""));

        if (depVal)
            for (auto i:_deps)
                RegisterFileDependency(depVal, i._filename.c_str());

        return _payload;
    }

        ////////////////////////////////////////////////////////////

    class ShaderCacheSet
    {
    public:
        std::shared_ptr<::Assets::ArchiveCache> GetArchive(
            const char shaderBaseFilename[], 
            const ::Assets::IntermediateResources::Store& intermediateStore);

        void LogStats(const ::Assets::IntermediateResources::Store& intermediateStore);

        ShaderCacheSet();
        ~ShaderCacheSet();
    protected:
        typedef std::pair<uint64, std::shared_ptr<::Assets::ArchiveCache>> Archive;
        std::vector<Archive> _archives;
        Threading::Mutex _archivesLock;
    };

    std::shared_ptr<::Assets::ArchiveCache> ShaderCacheSet::GetArchive(
        const char shaderBaseFilename[],
        const ::Assets::IntermediateResources::Store& intermediateStore)
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

    void ShaderCacheSet::LogStats(const ::Assets::IntermediateResources::Store& intermediateStore)
    {
            // log statistics information for all shaders in all archive caches
        uint64 totalShaderSize = 0; // in bytes
        uint64 totalAllocationSpace = 0;

        char baseDir[MaxPath];
        intermediateStore.MakeIntermediateName(baseDir, dimof(baseDir), "");
        auto baseDirLen = XlStringLen(baseDir);
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
            XlNormalizePath(buffer, dimof(buffer), buffer);

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
    
    std::shared_ptr<::Assets::PendingCompileMarker> OfflineCompileProcess::PrepareResource(
        uint64 typeCode, const ResChar* initializers[], unsigned initializerCount,
        const ::Assets::IntermediateResources::Store& destinationStore)
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

        auto shaderId = Metal::ShaderService::GetInstance().MakeResId(initializers[0]);

        char archiveName[MaxPath];
        _snprintf_s(archiveName, _TRUNCATE, "%s-%s", shaderId._filename, shaderId._shaderModel);

            //  assuming no hash conflicts here... Of course, with a very large number of shaders, 
            //  it's possible we could hit conflicts
        auto archiveId = Hash64(shaderId._entryPoint);
        const char* definesTable = (initializerCount > 1)?initializers[1]:nullptr;
        if (definesTable) {
            archiveId ^= Hash64(definesTable);
        }

        std::shared_ptr<::Assets::PendingCompileMarker> marker = nullptr;

        if (initializers[0] && initializers[0][0] != '\0' && XlCompareStringI(shaderId._filename, "null")!=0) {

                //  If this object already exists in the archive, and the dependencies are not
                //  invalidated, then we can load it immediately
                //  We can't rely on the dependencies being identical for each version of that
                //  shader... Sometimes there might be an #include that is hidden behind a #ifdef

            char depName[MaxPath];
            _snprintf_s(depName, _TRUNCATE, "%s-%08x%08x", archiveName, uint32(archiveId>>32ull), uint32(archiveId));

            auto archive = _shaderCacheSet->GetArchive(archiveName, destinationStore);
            if (archive->HasItem(archiveId)) {
                auto depVal = destinationStore.MakeDependencyValidation(depName);
                if (depVal) {
                    marker = std::make_shared<::Assets::PendingCompileMarker>(::Assets::AssetState::Ready, archiveName, archiveId, std::move(depVal));
                    marker->_archive = std::move(archive);
                }
            } 

            if (!marker) {
                marker = std::make_shared<::Assets::PendingCompileMarker>(::Assets::AssetState::Pending, archiveName, archiveId, nullptr);
                marker->_archive = archive;

                #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
                        //  When we have archive attachments enabled, we can write
                        //  some information to help identify this shader object
                        //  We'll start with something to define the object...
                    std::stringstream builder;
                    builder << "[" << shaderId._filename
                            << ":" << shaderId._entryPoint
                            << ":" << shaderId._shaderModel
                            << "] [" << (definesTable?definesTable:"") << "]";
                    auto archiveCacheAttachment = builder.str();
                #else
                    int archiveCacheAttachment;
                #endif

                using Payload = CompileHelper::Payload;

                std::string depNameAsString = depName;
                auto compileHelper = std::make_shared<CompileHelper>();

                static std::vector<std::shared_ptr<CompileHelper>> s_activeCompileOperations;
                s_activeCompileOperations.push_back(compileHelper);
                auto tempPtr = compileHelper.get();

                compileHelper->Enqueue(
                    shaderId, definesTable,
                    [marker, archiveCacheAttachment, depNameAsString, &destinationStore, tempPtr](
                        ::Assets::AssetState newState, const Payload& payload, 
                        const ::Assets::DependentFileState* depsBegin, const ::Assets::DependentFileState* depsEnd)
                    {
                        if (newState == ::Assets::AssetState::Ready && marker->_archive) {
                            assert(payload.get() && payload->size() > 0);

                            #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
                                auto metricsString = 
                                    Metal::ShaderService::GetInstance().GetLowLevelCompiler().MakeShaderMetricsString(
                                        AsPointer(payload->cbegin()), payload->size());
                            #endif

                            marker->_archive->Commit(
                                marker->_sourceID1, Payload(payload),
                                #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
                                    (archiveCacheAttachment + " [" + metricsString + "]")
                                #else
                                    std::string()
                                #endif
                                );
                            (void)archiveCacheAttachment;
                        }

                            // todo -- we should delay writing the deps file until we flush the cache to disk
                            //   -- todo -- "destinationStore" can be destroyed before we get here (if we shut down the game)
                        char baseDir[MaxPath];
                        XlDirname(baseDir, dimof(baseDir), marker->_sourceID0);
                        marker->_dependencyValidation = destinationStore.WriteDependencies(depNameAsString.c_str(), baseDir, depsBegin, depsEnd);

                            // give the PendingCompileMarker object the same state
                        marker->SetState(newState);

                        auto i = std::find_if(
                            s_activeCompileOperations.begin(), s_activeCompileOperations.end(), 
                            [tempPtr](std::shared_ptr<CompileHelper>& test) { return test.get() == tempPtr; });
                        if (i != s_activeCompileOperations.end()) s_activeCompileOperations.erase(i);
                    });
            }

            DEBUG_ONLY(marker->SetInitializer(initializers[0]));
        }

        return std::move(marker);
    }

    auto OfflineCompileProcess::CompileFromFile(
        const ResId& resId, 
        const ResChar definesTable[]) const -> std::shared_ptr<IPendingMarker>
    {
        auto compileHelper = std::make_shared<CompileHelper>();
        compileHelper->Enqueue(resId, definesTable, nullptr);
        return compileHelper;
    }
            
    auto OfflineCompileProcess::CompileFromMemory(
        const char shaderInMemory[], const char entryPoint[], 
        const char shaderModel[], const ResChar definesTable[]) const -> std::shared_ptr<IPendingMarker>
    {
        auto compileHelper = std::make_shared<CompileHelper>();
        compileHelper->Enqueue(shaderInMemory, entryPoint, shaderModel, definesTable); 
        return compileHelper;
    }

    OfflineCompileProcess::OfflineCompileProcess()
    {
        _shaderCacheSet = std::make_unique<ShaderCacheSet>();
    }

    OfflineCompileProcess::~OfflineCompileProcess()
    {}

}}

