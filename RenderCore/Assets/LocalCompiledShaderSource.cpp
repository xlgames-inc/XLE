// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LocalCompiledShaderSource.h"
#include "../Metal/Shader.h"

#include "../IDevice.h"
#include "../../Assets/ChunkFile.h"
#include "../../Assets/IArtifact.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/ArchiveCache.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/AsyncLoadOperation.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/CompilationThread.h"

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
    static const bool CompileInBackground = true;
    using ::Assets::ResChar;
    using ResId = ShaderService::ResId;

        ////////////////////////////////////////////////////////////

    class ShaderCacheSet
    {
    public:
        std::shared_ptr<::Assets::ArchiveCache> GetArchive(
            const char shaderBaseFilename[], 
            const ::Assets::IntermediateAssets::Store& intermediateStore);

        void LogStats(const ::Assets::IntermediateAssets::Store& intermediateStore);

        void Clear();
        
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
        return newArchive;
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

    void ShaderCacheSet::Clear() {
        ScopedLock(_archivesLock);
        _archives.clear();
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
		StringSection<Assets::ResChar> GetRequestParameters() const { return {}; }
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

    class LocalCompiledShaderSource::Marker : public ::Assets::IArtifactCompileMarker
    {
    public:
        std::shared_ptr<::Assets::IArtifact> GetExistingAsset() const;
        std::shared_ptr<::Assets::ArtifactFuture> InvokeCompile() const;
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
        if (!c) return nullptr;

		if (XlEqString(_res._filename, "null"))
			return std::make_shared<::Assets::BlobArtifact>(nullptr, std::make_shared<::Assets::DependencyValidation>());

        ::Assets::ResChar archiveName[MaxPath], depName[MaxPath];
        auto archiveId = GetTarget(_res, _definesTable, archiveName, dimof(archiveName), depName, dimof(depName));

        return std::make_shared<ArchivedFileArtifact>(
			c->_shaderCacheSet->GetArchive(archiveName, *_store), archiveId, 
			_store->MakeDependencyValidation(depName),
            nullptr, nullptr);
    }

    std::shared_ptr<::Assets::ArtifactFuture> LocalCompiledShaderSource::Marker::InvokeCompile() const
    {
        if (!_compiler.lock()) return nullptr;

        auto futureRes = std::make_shared<::Assets::ArtifactFuture>();

        auto store = _store;
        auto compiler = _compiler;
        auto definesTable = _definesTable;
        auto resId = _res;

        std::function<void(::Assets::ArtifactFuture&)> operation =
            [store, compiler, definesTable, resId] (::Assets::ArtifactFuture& future) {

            auto c = compiler.lock();
            if (!c)
                Throw(std::runtime_error("Low level shader compiler has been destroyed before compile operation completed"));

			std::vector<::Assets::DependentFileState> deps;
			Blob errors, payload;
			bool success = false;

			TRY
			{
				if (c->_preprocessor) {
					auto preprocessedOutput = c->_preprocessor->RunPreprocessor(resId._filename);
					if (preprocessedOutput._processedSource.empty())
						Throw(std::runtime_error("Preprocessed output is empty"));

					deps = std::move(preprocessedOutput._dependencies);

					success = c->_compiler->DoLowLevelCompile(
						payload, errors, deps,
						preprocessedOutput._processedSource.data(), preprocessedOutput._processedSource.size(), resId,
						definesTable.c_str(),
                        MakeIteratorRange(preprocessedOutput._lineMarkers));

				} else {
					deps.push_back(::Assets::IntermediateAssets::Store::GetDependentFileState(resId._filename));

					// Don't use TryLoadFileAsMemoryBlock here, because we want exceptions to propagate upwards
					// Also, allow read & write sharing, because we want to support rapid reloading of shaders that
					// might be open in an external editor
					auto file = ::Assets::MainFileSystem::OpenFileInterface(resId._filename, "rb" , FileShareMode::Read | FileShareMode::Write);
					file->Seek(0, FileSeekAnchor::End);
					auto fileSize = file->TellP();
					file->Seek(0);

					auto fileData = std::make_unique<uint8[]>(fileSize);
					file->Read(fileData.get(), 1, fileSize);

					success = c->_compiler->DoLowLevelCompile(
						payload, errors, deps,
						fileData.get(), fileSize, resId,
						definesTable.c_str());
				}
			}
				// embue any exceptions with the dependency validation
			CATCH(const ::Assets::Exceptions::ConstructionError& e)
			{
				Throw(::Assets::Exceptions::ConstructionError(e, ::Assets::AsDepVal(MakeIteratorRange(deps))));
			}
			CATCH(const std::exception& e)
			{
				Throw(::Assets::Exceptions::ConstructionError(e, ::Assets::AsDepVal(MakeIteratorRange(deps))));
			}
			CATCH_END

                // before we can finish the "complete" step, we need to commit
                // to archive output
            ::Assets::ResChar archiveName[MaxPath], depName[MaxPath];
            auto archiveId = GetTarget(
                resId, definesTable,
                archiveName, dimof(archiveName),
                depName, dimof(depName));
            auto archive = c->_shaderCacheSet->GetArchive(archiveName, *store);

			// payload will be empty on compile error, so don't write it out
            if (payload && !payload->empty()) {
				#if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
					auto metricsString =
						c->_compiler->MakeShaderMetricsString(
							AsPointer(payload->cbegin()), payload->size());
				#endif

				std::vector<::Assets::DependentFileState> depsAsVector(deps.begin(), deps.end());
				auto baseDirAsString = MakeFileNameSplitter(resId._filename).DriveAndPath().AsString();
				auto depNameAsString = std::string(depName);

				#if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
						//  When we have archive attachments enabled, we can write
						//  some information to help identify this shader object
						//  We'll start with something to define the object...
					std::stringstream builder;
					builder
						<< "[" << resId._filename
						<< ":" << resId._entryPoint
						<< ":" << resId._shaderModel
						<< "] [" << definesTable << "]";
					auto archiveCacheAttachment = builder.str();
				#endif

				archive->Commit(
					archiveId, payload,
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

                // Commit errors to disk if enabled
            if (c->_writeErrorLogFiles && errors && !errors->empty()) {
                auto hash = Hash64(AsPointer(errors->begin()), AsPointer(errors->end()));
                StringMeld<MaxPath> logFileName;
                auto splitter = MakeFileNameSplitter(resId._filename);
                logFileName << "shader_error/ShaderCompileError_" << splitter.File().AsString() << "_" << splitter.Extension().AsString() << "_" << std::hex << hash << ".txt";
                char finalLogFileName[MaxPath];
                store->MakeIntermediateName(finalLogFileName, logFileName.AsStringSection());
                RawFS::CreateDirectoryRecursive(MakeFileNameSplitter(finalLogFileName).DriveAndPath());
                auto file = ::Assets::MainFileSystem::OpenFileInterface(finalLogFileName, "wb");
                file->Write(errors->data(), errors->size());
                Log(Error) << "Debug log written to " << logFileName.get() << std::endl;
            }

                // Create the artifact and add it to the compile marker
			auto depVal = ::Assets::AsDepVal(MakeIteratorRange(deps));
			auto newState = success ? ::Assets::AssetState::Ready : ::Assets::AssetState::Invalid;

            future.AddArtifact("main", std::make_shared<::Assets::BlobArtifact>(payload, depVal));
            future.AddArtifact("log", std::make_shared<::Assets::BlobArtifact>(errors, depVal));

                // give the ArtifactFuture object the same state
            assert(future.GetArtifacts().size() != 0);
            future.SetState(newState);
        };

        if (CompileInBackground) {
            ::Assets::QueueCompileOperation(futureRes, std::move(operation));
        } else {
            operation(*futureRes);
        }

        return futureRes;
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
    
    std::shared_ptr<::Assets::IArtifactCompileMarker> LocalCompiledShaderSource::Prepare(
        uint64 typeCode, const StringSection<ResChar> initializers[], unsigned initializerCount)
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

        auto shaderId = ShaderService::MakeResId(initializers[0], _compiler.get());
		StringSection<ResChar> definesTable = (initializerCount > 1)?initializers[1]:StringSection<ResChar>();
        return std::make_shared<Marker>(initializers[0], shaderId, definesTable, *::Assets::Services::GetAsyncMan().GetIntermediateStore(), shared_from_this());
    }

    auto LocalCompiledShaderSource::CompileFromFile(
        StringSection<ResChar> resource, 
        StringSection<ResChar> definesTable) const -> std::shared_ptr<::Assets::ArtifactFuture>
    {
        /*auto compileHelper = std::make_shared<ShaderCompileMarker>(_compiler, _preprocessor);
        auto resId = ShaderService::MakeResId(resource, _compiler.get());
        compileHelper->Enqueue(resId, definesTable.AsString(), nullptr);
        return compileHelper;*/
        assert(0);
        return nullptr;
    }
            
    auto LocalCompiledShaderSource::CompileFromMemory(
        StringSection<char> shaderInMemory, StringSection<char> entryPoint, 
        StringSection<char> shaderModel, StringSection<ResChar> definesTable) const -> std::shared_ptr<::Assets::ArtifactFuture>
    {
        /*auto compileHelper = std::make_shared<ShaderCompileMarker>(_compiler, _preprocessor);
        compileHelper->Enqueue(shaderInMemory, entryPoint, shaderModel, definesTable); 
        return compileHelper;*/
        assert(0);
        return nullptr;
    }

    void LocalCompiledShaderSource::ClearCaches() {
        _shaderCacheSet->Clear();
    }
    
    void LocalCompiledShaderSource::StallOnPendingOperations(bool cancelAll)
    {
    }

    LocalCompiledShaderSource::LocalCompiledShaderSource(
        std::shared_ptr<ShaderService::ILowLevelCompiler> compiler,
        std::shared_ptr<ISourceCodePreprocessor> preprocessor,
        const DeviceDesc& devDesc)
    : _compiler(std::move(compiler))
    , _preprocessor(std::move(preprocessor))
    {
        _shaderCacheSet = std::make_unique<ShaderCacheSet>(devDesc);
    }

    LocalCompiledShaderSource::~LocalCompiledShaderSource()
    {
    }

}}

