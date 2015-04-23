// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Shader.h"
#include "DeviceContext.h"
#include "../../RenderUtils.h"
#include "../../../Assets/ChunkFile.h"
#include "../../../Assets/IntermediateResources.h"
#include "../../../Assets/CompileAndAsyncManager.h"
#include "../../../Assets/AssetUtils.h"
#include "../../../Assets/ArchiveCache.h"
#include "../../../Utility/Streams/PathUtils.h"
#include "../../../Utility/Streams/FileUtils.h"
#include "../../../Utility/SystemUtils.h"
#include "../../../Utility/IteratorUtils.h"
#include "../../../Utility/WinAPI/WinAPIWrapper.h"
#include "../../../ConsoleRig/Log.h"
#include <functional>
#include <deque>
#include <regex>

#include "IncludeDX11.h"
#include <D3D11Shader.h>
#include <D3Dcompiler.h>
#include <DxErr.h>
#include <D3DX11.h>

namespace RenderCore { 
    extern char VersionString[];
    extern char BuildDateString[];
}

namespace RenderCore { namespace Metal_DX11
{
    static const bool CompileInBackground = true;

    ID3DX11ThreadPump* GetThreadPump();
    void FlushThreadPump();

        ////////////////////////////////////////////////////////////

    static const char* AdaptShaderModel(const char inputShaderModel[])
    {
        if (!inputShaderModel || !inputShaderModel[0]) {
            return inputShaderModel;
        }

        size_t length = XlStringLen(inputShaderModel);

            //
            //      Some shaders end with vs_*, gs_*, etc..
            //      Change this to the highest shader model we can support
            //      with the current device
            //
        if (inputShaderModel[length-1] == '*') {
            auto featureLevel = ObjectFactory().GetUnderlying()->GetFeatureLevel();
            const char* bestShaderModel;
            if (featureLevel >= D3D_FEATURE_LEVEL_11_0)         { bestShaderModel = "5_0"; } 
            else if (featureLevel >= D3D_FEATURE_LEVEL_10_0)    { bestShaderModel = "4_0"; } 
            else if (featureLevel >= D3D_FEATURE_LEVEL_9_3)     { bestShaderModel = "4_0_level_9_3"; } 
            else if (featureLevel >= D3D_FEATURE_LEVEL_9_2)     { bestShaderModel = "4_0_level_9_2"; } 
            else                                                { bestShaderModel = "4_0_level_9_1"; }

            static char buffer[64];
            XlCopyString(buffer, inputShaderModel);
            buffer[std::min(length-1, dimof(buffer)-1)] = '\0';
            XlCatString(buffer, dimof(buffer), bestShaderModel);
            return buffer;      // note; returning pointer to static char buffer
        }

        return inputShaderModel;
    }

    static D3D10_SHADER_MACRO MakeShaderMacro(const char name[], const char definition[])
    {
        D3D10_SHADER_MACRO result;
        result.Name = name;
        result.Definition = definition;
        return result;
    }

    static UINT GetShaderCompilationFlags()
    {
        #if defined(_DEBUG)
            return D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION; //| D3D10_SHADER_WARNINGS_ARE_ERRORS;
        #else
            return D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_OPTIMIZATION_LEVEL3; // | D3D10_SHADER_NO_PRESHADER;
        #endif
    }

    class IncludeHandler : public ID3D10Include 
    {
    public:
        IncludeHandler(const char baseDirectory[], const Assets::DependentFileState& baseFile = Assets::DependentFileState()) : _baseDirectory(baseDirectory) 
        {
            _searchDirectories.push_back(baseDirectory);
            if (!baseFile._filename.empty()) {
                _includeFiles.push_back(baseFile);
            }
        }
        HRESULT __stdcall   Open(D3D10_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
        HRESULT __stdcall   Close(LPCVOID pData);
        
        const std::vector<Assets::DependentFileState>& GetIncludeFiles() const     { return _includeFiles; }
        const std::string&              GetBaseDirectory() const    { return _baseDirectory; }
        
    private:
        std::string                 _baseDirectory;
        std::vector<Assets::DependentFileState>    _includeFiles;
        std::vector<std::string>    _searchDirectories;
    };

    HRESULT     IncludeHandler::Open(D3D10_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
    {
        size_t size = 0;
        char path[MaxPath];
        for (auto i=_searchDirectories.cbegin(); i!=_searchDirectories.cend(); ++i) {
            XlCopyString(path, dimof(path), i->c_str());
            XlCatString(path, dimof(path), pFileName);
            XlSimplifyPath(path, dimof(path), path, "\\/");

            std::unique_ptr<uint8[]> file;
            ::Assets::DependentFileState timeMarker;
            {
                    // need to use Win32 file operations, so we can get the modification time
                    //  at exactly the time we're reading it.
                auto handle = CreateFile(
                    path, GENERIC_READ, FILE_SHARE_READ,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (handle != INVALID_HANDLE_VALUE) {
                    LARGE_INTEGER fileSize;
                    GetFileSizeEx(handle, &fileSize);
                    assert(!fileSize.HighPart); // not supporting very large files
                    size = fileSize.LowPart;

                    FILETIME ft;
                    GetFileTime(handle, nullptr, nullptr, &ft);

                    timeMarker._timeMarker = (uint64(ft.dwHighDateTime) << 32ull) | uint64(ft.dwLowDateTime);
                    file = std::make_unique<uint8[]>(size);
                    DWORD byteRead = 0;
                    auto b = ReadFile(handle, file.get(), DWORD(size), &byteRead, nullptr);
                    assert(b); (void)b;
                    CloseHandle(handle);
                }
            }

            if (file) {
                timeMarker._filename = path;
                
                XlDirname(path, dimof(path), path);
                std::string newDirectory = path;
                auto i = std::find(_searchDirectories.cbegin(), _searchDirectories.cend(), newDirectory);
                if (i==_searchDirectories.cend()) {
                    _searchDirectories.push_back(newDirectory);
                }
                if (ppData) { *ppData = file.release(); }
                if (pBytes) { *pBytes = (UINT)size; }

                _includeFiles.push_back(timeMarker);
                return S_OK;
            }
        }
        return -1;
    }

    HRESULT     IncludeHandler::Close(LPCVOID pData)
    {
        delete[] (uint8*)pData;
        return S_OK;
    }

    static std::vector<D3D10_SHADER_MACRO> MakeDefinesTable(const char definesTable[], const char shaderModel[], std::string& definesCopy)
    {
        char shaderModelDef[4] = "_SH";
        if (shaderModel)
            shaderModelDef[1] = (char)toupper(shaderModel[0]);

        definesCopy = definesTable?definesTable:std::string();
        unsigned definesCount = 1;
        size_t offset = 0;
        while ((offset = definesCopy.find_first_of(';', offset)) != std::string::npos) {
            ++definesCount; ++offset;
        }
            
        std::vector<D3D10_SHADER_MACRO> arrayOfDefines;
        arrayOfDefines.reserve(4+definesCount);
        arrayOfDefines.push_back(MakeShaderMacro("D3D11", "1"));
        #if defined(_DEBUG)
            arrayOfDefines.push_back(MakeShaderMacro("_DEBUG", "1"));
        #endif
        arrayOfDefines.push_back(MakeShaderMacro(shaderModelDef, "1"));

        offset = 0;
        if (!definesCopy.empty()) {
            for (;;) {
                size_t definition = definesCopy.find_first_of('=', offset);
                size_t defineEnd = definesCopy.find_first_of(';', offset);
                if (defineEnd == std::string::npos) {
                    defineEnd = definesCopy.size();
                }

                if (definition < defineEnd) {
                    definesCopy[definition] = '\0';
                    if (defineEnd < definesCopy.size()) {
                        definesCopy[defineEnd] = '\0';
                    }
                    arrayOfDefines.push_back(MakeShaderMacro(&definesCopy[offset], &definesCopy[definition+1]));
                } else {
                    arrayOfDefines.push_back(MakeShaderMacro(&definesCopy[offset], nullptr));
                }

                if (defineEnd+1 >= definesCopy.size()) {
                    break;
                }
                offset = defineEnd+1;
            }
        }
        arrayOfDefines.push_back(MakeShaderMacro(nullptr, nullptr));
        return arrayOfDefines;
    }

        ////////////////////////////////////////////////////////////

    class CompiledShaderByteCode::ShaderCompileHelper
    {
    public:
        intrusive_ptr<ID3D::Blob> Resolve(const char initializer[], const std::shared_ptr<Assets::DependencyValidation>& depVal = nullptr) const;

        const std::vector<Assets::DependentFileState>& GetDependencies() const     
        { 
            if (_includeHandler) {
                return _includeHandler->GetIncludeFiles(); 
            } else {
                static std::vector<Assets::DependentFileState> blank;
                return blank;
            }
        }

        ShaderCompileHelper(    const ShaderResId& shaderPath, const ResChar definesTable[], 
                                const std::shared_ptr<Assets::DependencyValidation>& depVal = nullptr);
        ShaderCompileHelper(    const char shaderInMemory[], const char entryPoint[], 
                                const char shaderModel[], const ResChar definesTable[]);
        ~ShaderCompileHelper();
    protected:
        mutable ID3D::Blob* _futureShader;      // (can't be intrusive_ptr because of use with D3DX11CompileFromFile)
        ID3D::Blob*         _futureErrors;
        HRESULT             _futureResult;

        mutable std::unique_ptr<IncludeHandler> _includeHandler;
        mutable std::unique_ptr<char[]>         _temporaryCompilingBuffer;

        ShaderCompileHelper(ShaderCompileHelper&);
        ShaderCompileHelper& operator=(const ShaderCompileHelper&);
    };
    
    CompiledShaderByteCode::ShaderCompileHelper::ShaderCompileHelper(
        const ShaderResId& shaderPath, const ResChar definesTable[], 
        const std::shared_ptr<Assets::DependencyValidation>& depVal)
    {
        _futureErrors = nullptr;
        _futureShader = nullptr;
        _futureResult = ~HRESULT(0x0);

            // DavidJ --    the normalize path steps here don't work for network
            //              paths. The first "\\" gets removed in the process
        ResChar normalizedPath  [MaxPath];
        XlNormalizePath(normalizedPath, dimof(normalizedPath), shaderPath._filename);
        
        ResChar directoryName[MaxPath];
        XlDirname(directoryName, dimof(directoryName), normalizedPath);
        auto includeHandler = std::make_unique<IncludeHandler>(
            directoryName, 
            ::Assets::CompileAndAsyncManager::GetInstance().GetIntermediateStore().GetDependentFileState(normalizedPath));

        std::string definesCopy;
        auto arrayOfDefines = MakeDefinesTable(definesTable, shaderPath._shaderModel, definesCopy);

            //
            //    Background compilation causes problem with GPA
            //        If we attempt to compile in the background with a
            //        thread pump, GPA will report that it can't find the HLSL
            //        source.
            //
        HRESULT hresult = D3DX11CompileFromFile(
            shaderPath._filename, AsPointer(arrayOfDefines.cbegin()), includeHandler.get(), 
            shaderPath._entryPoint, AdaptShaderModel(shaderPath._shaderModel),
            GetShaderCompilationFlags(),
            0, CompileInBackground ? GetThreadPump() : nullptr,
            &_futureShader, &_futureErrors, &_futureResult);

        _includeHandler = std::move(includeHandler);

        if (depVal) {
            RegisterFileDependency(depVal, normalizedPath);
        }

        if (!SUCCEEDED(hresult)) {
            if (_futureErrors && _futureErrors->GetBufferPointer()) {
                const char* errors = (const char*)_futureErrors->GetBufferPointer();
                OutputDebugString("Shader compile errors -- ");
                OutputDebugString(errors);
                    
                char buffer[512];
                _snprintf_s(buffer, _TRUNCATE, "Encountered shader compile errors for file (%s): \n%s", shaderPath._filename, errors);
                ThrowException(Assets::Exceptions::InvalidResource(shaderPath._filename, buffer));
            }
            ThrowException(Assets::Exceptions::InvalidResource(shaderPath._filename, "Unknown error"));
        }
    }

    CompiledShaderByteCode::ShaderCompileHelper::ShaderCompileHelper(
        const char shaderInMemory[], const char entryPoint[], const char shaderModel[], const ResChar definesTable[])
    {
        _futureErrors = nullptr;
        _futureShader = nullptr;
        _futureResult = ~HRESULT(0x0);

        auto includeHandler = std::make_unique<IncludeHandler>("");

        std::string definesCopy;
        auto arrayOfDefines = MakeDefinesTable(definesTable, shaderModel, definesCopy);

        size_t shaderBufferSize = XlStringLen(shaderInMemory);
        auto memoryBuffer = std::make_unique<char[]>(shaderBufferSize);
        XlCopyMemory(memoryBuffer.get(), shaderInMemory, shaderBufferSize);
        HRESULT hresult = D3DX11CompileFromMemory(
            memoryBuffer.get(), shaderBufferSize,
            "memory", AsPointer(arrayOfDefines.cbegin()), includeHandler.get(), 
            entryPoint, AdaptShaderModel(shaderModel),
            GetShaderCompilationFlags(),
            0, CompileInBackground ? GetThreadPump() : nullptr,
            &_futureShader, &_futureErrors, &_futureResult);

        if (!SUCCEEDED(hresult)) {
            if (_futureErrors && _futureErrors->GetBufferPointer()) {
                const char* errors = (const char*)_futureErrors->GetBufferPointer();
                OutputDebugString("Shader compile errors -- ");
                OutputDebugString(errors);
                
                char buffer[512];
                _snprintf_s(buffer, _TRUNCATE, "Encountered shader compile errors: \n%s", errors);
                ThrowException(Assets::Exceptions::InvalidResource("ShaderInMemory", buffer));
            }
            ThrowException(Assets::Exceptions::InvalidResource("ShaderInMemory", shaderInMemory));
        }

        _temporaryCompilingBuffer = std::move(memoryBuffer);
        _includeHandler = std::move(includeHandler);
    }

    CompiledShaderByteCode::ShaderCompileHelper::~ShaderCompileHelper()
    {
            //      We need to cancel the background compilation process, also
        while (_futureResult == HRESULT(~0)) { FlushThreadPump(); } // (best we can do now is just stall until we get a result)

        if (_futureShader) {
            _futureShader->Release();
            _futureShader = nullptr;
        }

        if (_futureErrors) {
            _futureErrors->Release();
            _futureErrors = nullptr;
        }
    }

    intrusive_ptr<ID3D::Blob> CompiledShaderByteCode::ShaderCompileHelper::Resolve(
        const char initializer[], const std::shared_ptr<Assets::DependencyValidation>& depVal) const
    {
        if (!_futureShader) {
            if (_futureResult == D3D11_ERROR_FILE_NOT_FOUND) {
                ThrowException(Assets::Exceptions::InvalidResource(initializer, "File not found"));
            }
            if (_futureResult!=~HRESULT(0x0)) {
                const char* errors = _futureErrors?((const char*)_futureErrors->GetBufferPointer()):nullptr;
                OutputDebugString("Shader compile errors -- ");
                OutputDebugString(errors);

                char buffer[512];
                _snprintf_s(buffer, _TRUNCATE, "Encountered shader compile errors: \n%s", errors);
                ThrowException(Assets::Exceptions::InvalidResource(initializer, buffer));
            }
            ThrowException(Assets::Exceptions::PendingResource(initializer, "Unknown error"));
        }

        if (_includeHandler) {

                //      If we've completed compiling, get the names of
                //      #included headers from the include handler, and setup
                //      dependency lists

            if (depVal) {
                auto includeFiles = _includeHandler->GetIncludeFiles();
                for (auto i=includeFiles.cbegin(); i!=includeFiles.cend(); ++i) {
                    RegisterFileDependency(depVal, i->_filename.c_str());
                }
            }

            // _includeHandler.reset();
        }
        _temporaryCompilingBuffer.reset();

            // we can "move" our pointer into the returned pointer -- we no longer need it in this class
        intrusive_ptr<ID3D::Blob> result(_futureShader, false);
        _futureShader = nullptr;
        return result;
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

    class ShaderCompileProcess : public ::Assets::IPollingAsyncProcess
    {
    public:
        Result::Enum Update();

        typedef CompiledShaderByteCode::ShaderCompileHelper CompileHelper;
        ShaderCompileProcess(
            const ShaderResId& initializer, const ResChar definesTable[], 
            std::shared_ptr<::Assets::ArchiveCache>&& archive, uint64 archiveId, CallbackFn&& fn);
        ~ShaderCompileProcess();
    protected:
        CompileHelper _compileHelper;
        std::shared_ptr<::Assets::ArchiveCache> _archive;
        uint64 _archiveId;

        DEBUG_ONLY(std::string _initializer;)
        const char* Initializer() const;

        #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
            std::string _archiveCacheAttachment;
        #endif
    };

    static HMODULE ShaderCompilerModule = (HMODULE)INVALID_HANDLE_VALUE;

    static HRESULT D3DReflect_Wrapper(
        const void* pSrcData, size_t SrcDataSize, 
        const IID& pInterface, void** ppReflector)
    {
        // This is a wrapper for the D3DReflect(). See D3D11CreateDevice_Wrapper in Device.cpp
        // for a similar function.
        // Note that if we open the module successfully, we will never close it!

        if (ShaderCompilerModule == INVALID_HANDLE_VALUE) {
            ShaderCompilerModule = (*Windows::Fn_LoadLibrary)("d3dcompiler_47.dll");
        }
        if (!ShaderCompilerModule || ShaderCompilerModule == INVALID_HANDLE_VALUE) {
			assert(0 && "d3dcompiler_47.dll is missing. Please make sure this dll is in the same directory as your executable, or reachable path");
            return E_NOINTERFACE;
        }

        typedef HRESULT WINAPI D3DReflect_Fn(LPCVOID, SIZE_T, REFIID, void**);

        auto fn = (D3DReflect_Fn*)(*Windows::Fn_GetProcAddress)(ShaderCompilerModule, "D3DReflect");
        if (!fn) {
            (*Windows::FreeLibrary)(ShaderCompilerModule);
            ShaderCompilerModule = (HMODULE)INVALID_HANDLE_VALUE;
            return E_NOINTERFACE;
        }

        return (*fn)(pSrcData, SrcDataSize, pInterface, ppReflector);
    }

    static std::string MakeShaderMetricsString(const void* data, size_t dataSize)
    {
            // build some metrics information about the given shader, using the D3D
            //  reflections interface.
        ID3D::ShaderReflection* reflTemp = nullptr;
        auto hresult = D3DReflect_Wrapper(data, dataSize, __uuidof(ID3D::ShaderReflection), (void**)&reflTemp);
        intrusive_ptr<ID3D::ShaderReflection> refl = moveptr(reflTemp);
        if (!SUCCEEDED(hresult) || !refl) {
            return "<Failure in D3DReflect>";
        }

        D3D11_SHADER_DESC desc;
        XlZeroMemory(desc);
        hresult = refl->GetDesc(&desc);
		if (!SUCCEEDED(hresult)) {
			return "<Failure in D3DReflect>";
		}

        std::stringstream str;
        str << "Instruction Count: " << desc.InstructionCount << "; ";
        str << "Temp Reg Count: " << desc.TempRegisterCount << "; ";
        str << "Temp Array Count: " << desc.TempArrayCount << "; ";
        str << "CB Count: " << desc.ConstantBuffers << "; ";
        str << "Res Count: " << desc.BoundResources << "; ";

        str << "Texture Instruction -- N:" << desc.TextureNormalInstructions 
            << " L:" << desc.TextureLoadInstructions 
            << " C:" << desc.TextureCompInstructions 
            << " B:" << desc.TextureBiasInstructions
            << " G:" << desc.TextureGradientInstructions
            << "; ";

        str << "Arith Instruction -- float:" << desc.FloatInstructionCount 
            << " i:" << desc.IntInstructionCount 
            << " uint:" << desc.FloatInstructionCount
            << "; ";

        str << "Flow control -- static:" << desc.StaticFlowControlCount
            << " dyn:" << desc.DynamicFlowControlCount
            << "; ";

        str << "Macro instructions:" << desc.MacroInstructionCount << "; ";

        str << "Compute shader instructions -- barrier:" << desc.cBarrierInstructions
            << " interlocked: " << desc.cInterlockedInstructions
            << " store: " << desc.cTextureStoreInstructions
            << "; ";

        str << "Bitwise instructions: " << refl->GetBitwiseInstructionCount() << "; ";
        str << "Conversion instructions: " << refl->GetConversionInstructionCount() << "; ";
        str << "Sample frequency: " << refl->IsSampleFrequencyShader();

        return str.str();
    }

    auto ShaderCompileProcess::Update() -> Result::Enum
    {
        TRY {
                //  Resolve the compilation process. Then write the
                //  result to the archive file. Finally, fire the trigger.
            auto resolved = _compileHelper.Resolve(Initializer());

                //  Write the results to the archive cache. The archive 
                //  probably won't flush to disk immediately, meaning that
                //  the pointer to the payload data should be retained in
                //  memory until the shader needs to load it.
            std::shared_ptr<std::vector<uint8>> payload;
            if (resolved) {
                auto ptr = resolved->GetBufferPointer();
                auto size = resolved->GetBufferSize();

                if (ptr && size) {
                    payload = std::make_shared<std::vector<uint8>>(
                        (const uint8*)ptr, PtrAdd((const uint8*)ptr, size));
                }
            }

            const bool isValid = payload && !payload->empty();
            _archive->Commit(
                _archiveId, Assets::ArchiveCache::BlockAndSize(payload),
                #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
                    _archiveCacheAttachment + " [" + MakeShaderMetricsString(AsPointer(payload->begin()), payload->size()) + "]"
                #else
                    std::string()
                #endif
                );

            FireTrigger(isValid ? ::Assets::AssetState::Ready : ::Assets::AssetState::Invalid, _compileHelper.GetDependencies());
            return Result::Finish;
        } CATCH (const Assets::Exceptions::PendingResource&) {
        } CATCH (const Assets::Exceptions::InvalidResource&) {
            FireTrigger(::Assets::AssetState::Invalid, _compileHelper.GetDependencies());
            return Result::Finish;
        } CATCH_END

        return Result::KeepPolling;
    }

    const char* ShaderCompileProcess::Initializer() const
    {
        #if defined(_DEBUG)
            return _initializer.c_str();
        #else
            return "";
        #endif
    }

    ShaderCompileProcess::ShaderCompileProcess(
        const ShaderResId& initializer, const ResChar definesTable[], 
        std::shared_ptr<::Assets::ArchiveCache>&& archive, uint64 archiveId, CallbackFn&& fn)
        : IPollingAsyncProcess(std::forward<CallbackFn>(fn))
        , _compileHelper(initializer, definesTable)
    {
        DEBUG_ONLY(_initializer = initializer._filename;)
        _archive = std::move(archive);
        _archiveId = archiveId;

        #if defined(ARCHIVE_CACHE_ATTACHED_STRINGS)
                //  When we have archive attachments enabled, we can write
                //  some information to help identify this shader object
                //  We'll start with something to define the object...
            _archiveCacheAttachment = 
                std::string("[") + initializer._filename
                + ":" + initializer._entryPoint
                + ":" + initializer._shaderModel
                + "] [" + (definesTable?definesTable:"") + "]";
        #endif
    }

    ShaderCompileProcess::~ShaderCompileProcess()
    {}

        ////////////////////////////////////////////////////////////

    namespace Internal { static ID3DX11ThreadPump* GlobalThreadPump = nullptr; }
    ID3DX11ThreadPump* GetThreadPump() { return Internal::GlobalThreadPump; }
    void FlushThreadPump()
    {
        Internal::GlobalThreadPump->WaitForAllItems();
        Internal::GlobalThreadPump->ProcessDeviceWorkItems(10000);
    }

    class ThreadPump_D3D : public ::Assets::IThreadPump
    {
    public:
        void Update();

        ThreadPump_D3D();
        ~ThreadPump_D3D();
    protected:
        intrusive_ptr<ID3DX11ThreadPump> _underlying;
    };

    void ThreadPump_D3D::Update()
    {
        _underlying->ProcessDeviceWorkItems(10);
    }

    ThreadPump_D3D::ThreadPump_D3D()
    {
        assert(Internal::GlobalThreadPump == nullptr);
        ID3DX11ThreadPump* temp = nullptr;
        D3DX11CreateThreadPump(0, 0, &temp);
        _underlying = moveptr(temp);

        Internal::GlobalThreadPump = _underlying.get();
    }

    ThreadPump_D3D::~ThreadPump_D3D() 
    {
        assert(Internal::GlobalThreadPump == _underlying.get());
        Internal::GlobalThreadPump = nullptr;
    }

        ////////////////////////////////////////////////////////////

    class OfflineCompileProcess 
        : public ::Assets::IntermediateResources::IResourceCompiler
        , public std::enable_shared_from_this<OfflineCompileProcess>
    {
    public:
        std::shared_ptr<::Assets::PendingCompileMarker> PrepareResource(
            uint64 typeCode, const ResChar* initializers[], unsigned initializerCount,
            const ::Assets::IntermediateResources::Store& destinationStore);

        ShaderCacheSet& GetCacheSet() { return *_shaderCacheSet; }

        static const uint64 Type_ShaderCompile = ConstHash64<'Shad', 'erCo', 'mpil', 'e'>::Value;

        OfflineCompileProcess();
        ~OfflineCompileProcess();
    protected:
        std::unique_ptr<ShaderCacheSet> _shaderCacheSet;
    };

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

        auto& man = ::Assets::CompileAndAsyncManager::GetInstance();

        ShaderResId shaderId(initializers[0]);

        {
                //  we have to do the "AdaptShaderModel" shader model here to convert
                //  the default shader model string (etc, "vs_*) to a resolved shader model
                //  this is because we want the archive name to be correct
            auto newShaderModel = AdaptShaderModel(shaderId._shaderModel);
            if (newShaderModel != shaderId._shaderModel) {
                XlCopyString(shaderId._shaderModel, newShaderModel);
            }
        }

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
                std::string depNameAsString = depName;
                XlDirname(depName, dimof(depName), archiveName);
                std::string baseDir = depName;
                man.Add(
                    std::make_shared<ShaderCompileProcess>(
                        shaderId,  definesTable,
                        std::move(archive), archiveId, 
                        [=](::Assets::AssetState::Enum newState, const std::vector<Assets::DependentFileState>& deps)
                        {
                                //  note -- we're accessing an unprotected pointer to the "destinationStore"
                                //  The list of dependencies here should mostly be path names relative
                                //  to the base file...
                            assert(!deps.empty());
                            marker->_dependencyValidation = destinationStore.WriteDependencies(depNameAsString.c_str(), baseDir.c_str(), deps);
                            marker->SetState(newState);
                        }));
            }

            DEBUG_ONLY(marker->SetInitializer(initializers[0]));
        }

        return std::move(marker);
    }

    OfflineCompileProcess::OfflineCompileProcess()
    {
        auto shaderCacheSet = std::make_unique<ShaderCacheSet>();
        _shaderCacheSet = std::move(shaderCacheSet);
    }

    OfflineCompileProcess::~OfflineCompileProcess()
    {
            // note --  we have to be careful when unloading this DLL!
            //          We may have created ID3D11Reflection objects using
            //          this dll. If any of them are still alive when we unload
            //          the DLL, then they will cause a crash if we attempt to
            //          use them, or call the destructor. The only way to be
            //          safe is to make sure all reflection objects are destroyed
            //          before unloading the dll
        if (ShaderCompilerModule != INVALID_HANDLE_VALUE) {
            (*Windows::FreeLibrary)(ShaderCompilerModule);
            ShaderCompilerModule = (HMODULE)INVALID_HANDLE_VALUE;
        }
    }

        ////////////////////////////////////////////////////////////

    std::unique_ptr<::Assets::CompileAndAsyncManager> CreateCompileAndAsyncManager()
    {
        auto result = std::make_unique<::Assets::CompileAndAsyncManager>();
        auto newProc = std::make_unique<OfflineCompileProcess>();
        // newProc->GetCacheSet().LogStats(result->GetIntermediateStore());
        result->GetIntermediateCompilers().AddCompiler(
            OfflineCompileProcess::Type_ShaderCompile, std::move(newProc));
        result->Add(std::make_unique<ThreadPump_D3D>());

        return std::move(result);
    }

        ////////////////////////////////////////////////////////////

    static ShaderStage::Enum AsShaderStage(const char shaderModel[])
    {
        switch (shaderModel[0]) {
        case 'v':   return ShaderStage::Vertex;
        case 'p':   return ShaderStage::Pixel;
        case 'g':   return ShaderStage::Geometry;
        case 'd':   return ShaderStage::Domain;
        case 'h':   return ShaderStage::Hull;
        case 'c':   return ShaderStage::Compute;
        default:    return ShaderStage::Null;
        }
    }

    CompiledShaderByteCode::CompiledShaderByteCode(std::shared_ptr<::Assets::PendingCompileMarker>&& marker)
    {
            // no way to know the shader stage in this mode...
            //  Maybe the shader stage should be encoded in the intermediate file name
        _stage = ShaderStage::Null;
        DEBUG_ONLY(_initializer[0] = '\0');
        auto validationCallback = std::make_shared<Assets::DependencyValidation>();

        if (marker) {
            auto i = strrchr(marker->_sourceID0, '-');
            if (i) {
                _stage = AsShaderStage(i+1);
            }
            DEBUG_ONLY(XlCopyString(_initializer, marker->Initializer()));

                // if immediately ready, we can do a resolve right now.
            if (marker->GetState() == ::Assets::AssetState::Invalid) {
                ThrowException(Assets::Exceptions::InvalidResource(Initializer(), ""));
            } else if (marker->GetState() == ::Assets::AssetState::Ready) {
                if (marker->_archive) {
                    TRY {
                        _shader1 = marker->_archive->OpenFromCache(marker->_sourceID1);
                    } CATCH (...) {
                        ThrowException(Assets::Exceptions::InvalidResource(Initializer(), ""));
                    } CATCH_END
                }

                if (!_shader1 || _shader1->empty()) {
                    ThrowException(Assets::Exceptions::InvalidResource(Initializer(), ""));
                }

                if (marker->_dependencyValidation) {
                    Assets::RegisterAssetDependency(validationCallback, marker->_dependencyValidation.get());
                }
                marker.reset();
            }
        }

        _marker = std::move(marker);
        _validationCallback = std::move(validationCallback);
    }

    CompiledShaderByteCode::CompiledShaderByteCode(const ResChar initializer[], const ResChar definesTable[])
    {
        _stage = ShaderStage::Null;
        auto validationCallback = std::make_shared<Assets::DependencyValidation>();
        std::unique_ptr<ShaderCompileHelper> compileHelper;
        DEBUG_ONLY(XlCopyString(_initializer, initializer);)

        if (initializer && initializer[0] != '\0') {
            ShaderResId shaderPath(initializer);
            if (XlCompareStringI(shaderPath._filename, "null")!=0) {
                _stage = AsShaderStage(shaderPath._shaderModel);
                compileHelper = std::make_unique<ShaderCompileHelper>(shaderPath, definesTable, validationCallback);
            }
        }

        _validationCallback = std::move(validationCallback);
        _compileHelper = std::move(compileHelper);
    }

    CompiledShaderByteCode::CompiledShaderByteCode(const char shaderInMemory[], const char entryPoint[], const char shaderModel[], const ResChar definesTable[])
    {
        _stage = AsShaderStage(shaderModel);
        auto validationCallback = std::make_shared<Assets::DependencyValidation>();
        DEBUG_ONLY(XlCopyString(_initializer, "ShaderInMemory");)
        auto compileHelper = std::make_unique<ShaderCompileHelper>(shaderInMemory, entryPoint, shaderModel, definesTable);

        _validationCallback = std::move(validationCallback);
        _compileHelper = std::move(compileHelper);
    }

    CompiledShaderByteCode::~CompiledShaderByteCode()
    {
    }

    void CompiledShaderByteCode::Resolve() const
    {
        if (_compileHelper) {
                //  compile helper will either return a completed blob,
                //  or throw and exception
            _shader = _compileHelper->Resolve(Initializer(), _validationCallback);
            _compileHelper.reset();
        } else if (_marker) {
            if (_marker->GetState() == ::Assets::AssetState::Pending) {
                throw Assets::Exceptions::PendingResource(Initializer(), "");
            } else if (_marker->GetState() != ::Assets::AssetState::Invalid) {
                    //  Our shader should be stored in a shader cache file
                    //  Find that file, and get the completed shader.
                    //  Note that this might hit the disk currently...?
                if (_marker->_archive) {
                    TRY {
                        _shader1 = _marker->_archive->OpenFromCache(_marker->_sourceID1);
                    } CATCH (...) {
                        LogWarning << "Compilation marker is finished, but shader couldn't be opened from cache (" << _marker->_sourceID0 << ":" <<_marker->_sourceID1 << ")";
                    } CATCH_END
                }
            }

                //  Even when we're considered invalid, we must register a dependency, and release
                //  the marker. This way we will be recompiled if the asset is changed (eg, to fix
                //  the compile error)
            if (_marker->_dependencyValidation) {
                Assets::RegisterAssetDependency(_validationCallback, _marker->_dependencyValidation.get());
            }
            _marker.reset();
        }

        if ((!_shader1 || _shader1->empty()) && !_shader) {
            throw Assets::Exceptions::InvalidResource(Initializer(), "");
        }
    }

    const void* CompiledShaderByteCode::GetByteCode() const
    {
        Resolve();
        if (_shader1) {
            return AsPointer(_shader1->begin());    
        }
        return _shader->GetBufferPointer();
    }

    size_t CompiledShaderByteCode::GetSize() const
    {
        Resolve();
        if (_shader1) {
            return _shader1->size();
        }
        return _shader->GetBufferSize();
    }

    intrusive_ptr<ID3D::ShaderReflection> CompiledShaderByteCode::GetReflection() const
    {
        if (_stage == ShaderStage::Null) {
            return intrusive_ptr<ID3D::ShaderReflection>();
        }

        ID3D::ShaderReflection* reflectionTemp = nullptr;
        HRESULT hresult = D3DReflect_Wrapper(GetByteCode(), GetSize(), __uuidof(ID3D::ShaderReflection), (void**)&reflectionTemp);
        if (!SUCCEEDED(hresult) || !reflectionTemp) {
            ThrowException(Assets::Exceptions::InvalidResource(Initializer(), 
                (hresult == E_NOINTERFACE)?"Could not find d3dcompiler_47.dll":"Failure creating reflection"));
        }
        return moveptr(reflectionTemp);
    }

    const char*                     CompiledShaderByteCode::Initializer() const
    {
        #if defined(_DEBUG)
            return _initializer;
        #else
            return "";
        #endif
    }


    const uint64 CompiledShaderByteCode::CompileProcessType = OfflineCompileProcess::Type_ShaderCompile;


    
}}


