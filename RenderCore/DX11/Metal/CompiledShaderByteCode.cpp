// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Shader.h"
#include "DeviceContext.h"
#include "../../../Assets/IntermediateResources.h"
#include "../../../Assets/ArchiveCache.h"
#include "../../../Assets/AssetUtils.h"
#include "../../../Assets/InvalidAssetManager.h"
#include "../../../Utility/Streams/PathUtils.h"
#include "../../../Utility/Threading/Mutex.h"
#include "../../../Utility/StringFormat.h"

#include "../../../Utility/WinAPI/WinAPIWrapper.h"
#include "../../../ConsoleRig/Log.h"
#include "IncludeDX11.h"
#include <D3D11Shader.h>
#include <D3Dcompiler.h>
#include <DxErr.h>

namespace RenderCore { namespace Metal_DX11
{
    using ::Assets::ResChar;

    static HRESULT D3DCompile_Wrapper(
        LPCVOID pSrcData,
        SIZE_T SrcDataSize,
        LPCSTR pSourceName,
        const D3D_SHADER_MACRO* pDefines,
        ID3DInclude* pInclude,
        LPCSTR pEntrypoint,
        LPCSTR pTarget,
        UINT Flags1,
        UINT Flags2,
        ID3DBlob** ppCode,
        ID3DBlob** ppErrorMsgs);

    class D3DShaderCompiler : public ShaderService::ILowLevelCompiler
    {
    public:
        virtual void AdaptShaderModel(
            ResChar destination[], 
            const size_t destinationCount,
            const ResChar source[]) const;

        virtual bool DoLowLevelCompile(
            /*out*/ Payload& payload,
            /*out*/ Payload& errors,
            /*out*/ std::vector<::Assets::DependentFileState>& dependencies,
            const void* sourceCode, size_t sourceCodeLength,
            ShaderService::ResId& shaderPath,
            const ::Assets::rstring& definesTable) const;

        virtual std::string MakeShaderMetricsString(
            const void* byteCode, size_t byteCodeSize) const;

        HRESULT D3DReflect_Wrapper(
            const void* pSrcData, size_t SrcDataSize, 
            const IID& pInterface, void** ppReflector) const;

        HRESULT D3DCompile_Wrapper(
            LPCVOID pSrcData,
            SIZE_T SrcDataSize,
            LPCSTR pSourceName,
            const D3D_SHADER_MACRO* pDefines,
            ID3DInclude* pInclude,
            LPCSTR pEntrypoint,
            LPCSTR pTarget,
            UINT Flags1,
            UINT Flags2,
            ID3DBlob** ppCode,
            ID3DBlob** ppErrorMsgs) const;

        D3DShaderCompiler();
        ~D3DShaderCompiler();

    protected:
        mutable Threading::Mutex _moduleLock;
        mutable HMODULE _module;
        HMODULE GetShaderCompileModule() const;
    };

        ////////////////////////////////////////////////////////////

    void D3DShaderCompiler::AdaptShaderModel(
        ResChar destination[], 
        const size_t destinationCount,
        const ResChar inputShaderModel[]) const
    {
        assert(inputShaderModel);
        if (inputShaderModel[0] != '\0') {
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
            
                if (destination != inputShaderModel) 
                    XlCopyString(destination, destinationCount, inputShaderModel);

                destination[std::min(length-1, destinationCount-1)] = '\0';
                XlCatString(destination, destinationCount, bestShaderModel);
                return;
            }
        }

        if (destination != inputShaderModel) 
            XlCopyString(destination, destinationCount, inputShaderModel);
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
        HRESULT __stdcall   Open(D3D10_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
        HRESULT __stdcall   Close(LPCVOID pData);
        
        const std::vector<::Assets::DependentFileState>& GetIncludeFiles() const { return _includeFiles; }
        const std::string& GetBaseDirectory() const { return _baseDirectory; }

        IncludeHandler(const char baseDirectory[]) : _baseDirectory(baseDirectory) 
        {
            _searchDirectories.push_back(baseDirectory);
        }
        ~IncludeHandler() {}
    private:
        std::string _baseDirectory;
        std::vector<::Assets::DependentFileState> _includeFiles;
        std::vector<std::string> _searchDirectories;
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
                    // only add this to the list of include file, if it doesn't
                    // already exist there. We will get repeats and when headers
                    // are included multiple times (#pragma once isn't supported by
                    // the HLSL compiler)
                auto existing = std::find_if(_includeFiles.cbegin(), _includeFiles.cend(),
                    [&path](const ::Assets::DependentFileState& depState)
                    {
                        return !XlCompareStringI(depState._filename.c_str(), path);
                    });

                if (existing == _includeFiles.cend()) {
                    timeMarker._filename = path;
                    _includeFiles.push_back(timeMarker);
                    
                    XlDirname(path, dimof(path), path);
                    std::string newDirectory = path;
                    auto i = std::find(_searchDirectories.cbegin(), _searchDirectories.cend(), newDirectory);
                    if (i==_searchDirectories.cend()) {
                        _searchDirectories.push_back(newDirectory);
                    }
                }

                if (ppData) { *ppData = file.release(); }
                if (pBytes) { *pBytes = (UINT)size; }

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

    static const char s_shaderModelDef_V[] = "VSH";
    static const char s_shaderModelDef_P[] = "PSH";
    static const char s_shaderModelDef_G[] = "GSH";
    static const char s_shaderModelDef_H[] = "HSH";
    static const char s_shaderModelDef_D[] = "DSH";
    static const char s_shaderModelDef_C[] = "CSH";

    static std::vector<D3D10_SHADER_MACRO> MakeDefinesTable(const char definesTable[], const char shaderModel[], std::string& definesCopy)
    {
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

        const char* shaderModelStr = nullptr;
        switch (tolower(shaderModel[0])) {
        case 'v': shaderModelStr = s_shaderModelDef_V; break;
        case 'p': shaderModelStr = s_shaderModelDef_P; break;
        case 'g': shaderModelStr = s_shaderModelDef_G; break;
        case 'h': shaderModelStr = s_shaderModelDef_H; break;
        case 'd': shaderModelStr = s_shaderModelDef_D; break;
        case 'c': shaderModelStr = s_shaderModelDef_C; break;
        }
        if (shaderModelStr)
            arrayOfDefines.push_back(MakeShaderMacro(shaderModelStr, "1"));

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

    #define RECORD_INVALID_ASSETS
    #if defined(RECORD_INVALID_ASSETS)

        static void RegisterInvalidAsset(const ::Assets::ResChar assetName[], const std::basic_string<::Assets::ResChar>& errorString)
        {
            ::Assets::Services::GetInvalidAssetMan().MarkInvalid(assetName, errorString);
        }

        static void RegisterValidAsset(const ::Assets::ResChar assetName[])
        {
            ::Assets::Services::GetInvalidAssetMan().MarkValid(assetName);
        }

    #else

        static void RegisterInvalidAsset(const ResChar resourceName[], const std::basic_string<ResChar>& errorString) {}
        static void RegisterValidAsset(const ResChar resourceName[]) {}

    #endif

    static const char* AsString(HRESULT reason)
    {
        switch (reason) {
        case D3D11_ERROR_FILE_NOT_FOUND: return "File not found";
        case E_NOINTERFACE: return "Could not find d3dcompiler_47.dll";
        default: return "General failure";
        }
    }

    static void MarkInvalid(
        const ShaderService::ResId& shaderPath,
        HRESULT hresult,
        const ShaderService::ILowLevelCompiler::Payload& errorsBlob)
    {
        StringMeld<MaxPath, ::Assets::ResChar> initializer;
        initializer << shaderPath._filename << ':' << shaderPath._entryPoint << ':' << shaderPath._shaderModel;

        std::basic_stringstream<::Assets::ResChar> stream;
        stream << "Encountered errors while compiling shader: " << initializer << " (" << AsString(hresult) << ")" << std::endl;

        if (errorsBlob && !errorsBlob->empty()) {
            const auto* errors = (const char*)AsPointer(errorsBlob->cbegin());
            stream << "Errors as follows:" << std::endl;
            stream << errors;
        } else {
            stream << "(no extra data)" << std::endl;
        }

        RegisterInvalidAsset(initializer, stream.str());
    }
  
    static void MarkValid(const ShaderService::ResId& shaderPath)
    {
        RegisterValidAsset(
            StringMeld<MaxPath, ::Assets::ResChar>() << shaderPath._filename << ':' << shaderPath._entryPoint << ':' << shaderPath._shaderModel);
    }

    bool D3DShaderCompiler::DoLowLevelCompile(
        /*out*/ std::shared_ptr<std::vector<uint8>>& payload,
        /*out*/ std::shared_ptr<std::vector<uint8>>& errors,
        /*out*/ std::vector<::Assets::DependentFileState>& dependencies,
        const void* sourceCode, size_t sourceCodeLength,
        ShaderService::ResId& shaderPath,
        const ::Assets::rstring& definesTable) const
    {
            // This is called (typically in a background thread)
            // after the shader data has been loaded from disk.
            // Here we will invoke the D3D compiler. It will block
            // in this thread (so we should normally only call this from
            // a background thread (often one in a thread pool)
        ID3DBlob* codeResult = nullptr, *errorResult = nullptr;

        std::string definesCopy;
        auto arrayOfDefines = MakeDefinesTable(definesTable.c_str(), shaderPath._shaderModel, definesCopy);

        ::Assets::ResChar directoryName[MaxPath];
        XlDirname(directoryName, dimof(directoryName), shaderPath._filename);
        IncludeHandler includeHandler(directoryName);

        ResChar shaderModel[64];
        AdaptShaderModel(shaderModel, dimof(shaderModel), shaderPath._shaderModel);

        auto hresult = D3DCompile_Wrapper(
            sourceCode, sourceCodeLength,
            shaderPath._filename,

            AsPointer(arrayOfDefines.cbegin()), &includeHandler, 
            shaderPath._entryPoint, shaderModel,

            GetShaderCompilationFlags(), 0, 
            &codeResult, &errorResult);

            // we get a "blob" from D3D. But we need to copy it into
            // a shared_ptr<vector> so we can pass to it our clients
        payload.reset();
        if (codeResult && codeResult->GetBufferPointer() && codeResult->GetBufferSize()) {
            payload = std::make_shared<std::vector<uint8>>(
                (uint8*)codeResult->GetBufferPointer(), 
                PtrAdd((uint8*)codeResult->GetBufferPointer(), codeResult->GetBufferSize()));
        }

        errors.reset();
        if (errorResult && errorResult->GetBufferPointer() && errorResult->GetBufferSize()) {
            errors = std::make_shared<std::vector<uint8>>(
                (uint8*)errorResult->GetBufferPointer(), 
                PtrAdd((uint8*)errorResult->GetBufferPointer(), errorResult->GetBufferSize()));
        }

        dependencies = includeHandler.GetIncludeFiles();
        auto& intStore = ::Assets::Services::GetAsyncMan().GetIntermediateStore();
        dependencies.push_back(intStore.GetDependentFileState(shaderPath._filename));   // also need a dependency for the base file

        if (SUCCEEDED(hresult)) {
            MarkValid(shaderPath);
        } else {
            MarkInvalid(shaderPath, hresult, errors);
        }

        return SUCCEEDED(hresult);
    }
    
        ////////////////////////////////////////////////////////////

    HMODULE D3DShaderCompiler::GetShaderCompileModule() const
    {
        ScopedLock(_moduleLock);
        if (_module == INVALID_HANDLE_VALUE)
            _module = (*Windows::Fn_LoadLibrary)("d3dcompiler_47.dll");
        return _module;
    }

    HRESULT D3DShaderCompiler::D3DReflect_Wrapper(
        const void* pSrcData, size_t SrcDataSize, 
        const IID& pInterface, void** ppReflector) const
    {
            // This is a wrapper for the D3DReflect(). See D3D11CreateDevice_Wrapper in Device.cpp
            // for a similar function.

        auto compiler = GetShaderCompileModule();
        if (!compiler || compiler == INVALID_HANDLE_VALUE) {
			assert(0 && "d3dcompiler_47.dll is missing. Please make sure this dll is in the same directory as your executable, or reachable path");
            return E_NOINTERFACE;
        }

        typedef HRESULT WINAPI D3DReflect_Fn(LPCVOID, SIZE_T, REFIID, void**);

        auto fn = (D3DReflect_Fn*)(*Windows::Fn_GetProcAddress)(compiler, "D3DReflect");
        if (!fn) {
            (*Windows::FreeLibrary)(compiler);
            compiler = (HMODULE)INVALID_HANDLE_VALUE;
            return E_NOINTERFACE;
        }

        return (*fn)(pSrcData, SrcDataSize, pInterface, ppReflector);
    }

    HRESULT D3DShaderCompiler::D3DCompile_Wrapper(
        LPCVOID pSrcData,
        SIZE_T SrcDataSize,
        LPCSTR pSourceName,
        const D3D_SHADER_MACRO* pDefines,
        ID3DInclude* pInclude,
        LPCSTR pEntrypoint,
        LPCSTR pTarget,
        UINT Flags1,
        UINT Flags2,
        ID3DBlob** ppCode,
        ID3DBlob** ppErrorMsgs) const
    {
            // This is a wrapper for the D3DReflect(). See D3D11CreateDevice_Wrapper in Device.cpp
            // for a similar function.

        auto compiler = GetShaderCompileModule();
        if (!compiler || compiler == INVALID_HANDLE_VALUE) {
			assert(0 && "d3dcompiler_47.dll is missing. Please make sure this dll is in the same directory as your executable, or reachable path");
            return E_NOINTERFACE;
        }

        typedef HRESULT WINAPI D3DCompile_Fn(
            LPCVOID, SIZE_T, LPCSTR,
            const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR,
            UINT, UINT, ID3DBlob**, ID3DBlob**);

        auto fn = (D3DCompile_Fn*)(*Windows::Fn_GetProcAddress)(compiler, "D3DCompile");
        if (!fn) {
            (*Windows::FreeLibrary)(compiler);
            compiler = (HMODULE)INVALID_HANDLE_VALUE;
            return E_NOINTERFACE;
        }

        return (*fn)(pSrcData, SrcDataSize, pSourceName, pDefines, pInclude, pEntrypoint, pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);
    }

    std::string D3DShaderCompiler::MakeShaderMetricsString(const void* data, size_t dataSize) const
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

    D3DShaderCompiler::D3DShaderCompiler() 
    {
        _module = (HMODULE)INVALID_HANDLE_VALUE;
    }

    D3DShaderCompiler::~D3DShaderCompiler()
    {
            // note --  we have to be careful when unloading this DLL!
            //          We may have created ID3D11Reflection objects using
            //          this dll. If any of them are still alive when we unload
            //          the DLL, then they will cause a crash if we attempt to
            //          use them, or call the destructor. The only way to be
            //          safe is to make sure all reflection objects are destroyed
            //          before unloading the dll
        if (_module != INVALID_HANDLE_VALUE) {
            (*Windows::FreeLibrary)(_module);
            _module = (HMODULE)INVALID_HANDLE_VALUE;
        }
    }

    auto ShaderService::MakeResId(const char initializer[]) -> ResId
    {
        ResId shaderId;
        
                    // (convert unicode string -> single width char for entrypoint & shadermodel)

        const ::Assets::ResChar* endFileName = Utility::XlFindChar(initializer, ':');
        const ::Assets::ResChar* startShaderModel = nullptr;
        if (!endFileName) {
            XlCopyString(shaderId._filename, initializer);
            XlCopyString(shaderId._entryPoint, "main");
        } else {
            XlCopyNString(shaderId._filename, initializer, endFileName - initializer);
            startShaderModel = Utility::XlFindChar(endFileName+1, ':');

            if (!startShaderModel) {
                XlCopyString(shaderId._entryPoint, endFileName+1);
            } else {
                XlCopyNString(shaderId._entryPoint, endFileName+1, startShaderModel - endFileName - 1);
                XlCopyString(shaderId._shaderModel, startShaderModel+1);
            }
        }

        if (!startShaderModel)
            XlCopyString(shaderId._shaderModel, PS_DefShaderModel);

            //  we have to do the "AdaptShaderModel" shader model here to convert
            //  the default shader model string (etc, "vs_*) to a resolved shader model
            //  this is because we want the archive name to be correct
        _compiler->AdaptShaderModel(shaderId._shaderModel, dimof(shaderId._shaderModel), shaderId._shaderModel);

        return std::move(shaderId);
    }

    auto ShaderService::CompileFromFile(
        const ResId& resId, 
        const ::Assets::ResChar definesTable[]) const -> std::shared_ptr<IPendingMarker>
    {
        for (const auto& i:_shaderSources) {
            auto r = i->CompileFromFile(resId, definesTable);
            if (r) return r;
        }
        return nullptr;
    }

    auto ShaderService::CompileFromMemory(
        const char shaderInMemory[], 
        const char entryPoint[], const char shaderModel[], 
        const ::Assets::ResChar definesTable[]) const -> std::shared_ptr<IPendingMarker>
    {
        for (const auto& i:_shaderSources) {
            auto r = i->CompileFromMemory(shaderInMemory, entryPoint, shaderModel, definesTable);
            if (r) return r;
        }
        return nullptr;
    }

    void ShaderService::AddShaderSource(std::shared_ptr<IShaderSource> shaderSource)
    {
        _shaderSources.push_back(shaderSource);
    }

    void ShaderService::SetLowLevelCompiler(std::shared_ptr<ILowLevelCompiler> compiler)
    {
        _compiler = std::move(compiler);
    }

    ShaderService* ShaderService::s_instance = nullptr;
    void ShaderService::SetInstance(ShaderService* instance)
    {
        if (instance) {
            assert(!s_instance);
            s_instance = instance;
        } else {
            s_instance = nullptr;
        }
    }

    ShaderService::ShaderService() {}
    ShaderService::~ShaderService() {}

    ShaderService::IPendingMarker::~IPendingMarker() {}
    ShaderService::IShaderSource::~IShaderSource() {}
    ShaderService::ILowLevelCompiler::~ILowLevelCompiler() {}

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
        _validationCallback = std::make_shared<Assets::DependencyValidation>();

        if (marker) {
            auto i = strrchr(marker->_sourceID0, '-');
            if (i) _stage = AsShaderStage(i+1);
            DEBUG_ONLY(XlCopyString(_initializer, marker->Initializer()));
            _marker = std::move(marker);

                // if immediately ready, we can do a resolve right now.
            if (_marker->GetState() != ::Assets::AssetState::Pending) {
                ResolveFromCompileMarker();
                if (!_shader || _shader->empty())
                    ThrowException(Assets::Exceptions::InvalidResource(Initializer(), ""));
            }
        }
    }

    CompiledShaderByteCode::CompiledShaderByteCode(const ::Assets::ResChar initializer[], const ::Assets::ResChar definesTable[])
    {
        _stage = ShaderStage::Null;
        auto validationCallback = std::make_shared<Assets::DependencyValidation>();
        std::shared_ptr<ShaderService::IPendingMarker> compileHelper;
        DEBUG_ONLY(XlCopyString(_initializer, initializer);)

        if (initializer && initializer[0] != '\0') {
            auto shaderPath = ShaderService::GetInstance().MakeResId(initializer);
            if (XlCompareStringI(shaderPath._filename, "null")!=0) {
                _stage = AsShaderStage(shaderPath._shaderModel);
                compileHelper = 
                    ShaderService::GetInstance().CompileFromFile(
                        shaderPath, definesTable);

                RegisterFileDependency(validationCallback, shaderPath._filename);
            }
        }

        _validationCallback = std::move(validationCallback);
        _compileHelper = std::move(compileHelper);
    }

    CompiledShaderByteCode::CompiledShaderByteCode(
        const char shaderInMemory[], const char entryPoint[], 
        const char shaderModel[], const ::Assets::ResChar definesTable[])
    {
        _stage = AsShaderStage(shaderModel);
        auto validationCallback = std::make_shared<Assets::DependencyValidation>();
        DEBUG_ONLY(XlCopyString(_initializer, "ShaderInMemory");)
        auto compileHelper = 
            ShaderService::GetInstance().CompileFromMemory(
                shaderInMemory, entryPoint, shaderModel, definesTable);

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
            if (_marker->GetState() == ::Assets::AssetState::Pending)
                ThrowException(Assets::Exceptions::PendingResource(Initializer(), ""));
            
            ResolveFromCompileMarker();
        }

        if (!_shader || _shader->empty())
            ThrowException(Assets::Exceptions::InvalidResource(Initializer(), ""));
    }

    void CompiledShaderByteCode::ResolveFromCompileMarker() const
    {
        if (_marker->GetState() != ::Assets::AssetState::Invalid) {
                //  Our shader should be stored in a shader cache file
                //  Find that file, and get the completed shader.
                //  Note that this might hit the disk currently...?
            if (_marker->_archive) {
                TRY {
                    _shader = _marker->_archive->OpenFromCache(_marker->_sourceID1);
                } CATCH (...) {
                    LogWarning << "Compilation marker is finished, but shader couldn't be opened from cache (" << _marker->_sourceID0 << ":" <<_marker->_sourceID1 << ")";
                } CATCH_END
            }
        }

            //  Even when we're considered invalid, we must register a dependency, and release
            //  the marker. This way we will be recompiled if the asset is changed (eg, to fix
            //  the compile error)
        if (_marker->_dependencyValidation)
            Assets::RegisterAssetDependency(_validationCallback, _marker->_dependencyValidation);

        if (_marker->GetState() == ::Assets::AssetState::Invalid)
            ThrowException(Assets::Exceptions::InvalidResource(Initializer(), ""));

        _marker.reset();
    }

    const void* CompiledShaderByteCode::GetByteCode() const
    {
        Resolve();
        return AsPointer(_shader->begin());
    }

    size_t CompiledShaderByteCode::GetSize() const
    {
        Resolve();
        return _shader->size();
    }

    intrusive_ptr<ID3D::ShaderReflection> CompiledShaderByteCode::GetReflection() const
    {
        if (_stage == ShaderStage::Null)
            return intrusive_ptr<ID3D::ShaderReflection>();

            // awkward --   here we need to upcast to a d3d compiler in order to
            //              get access to the "D3DReflect_Wrapper" function.
            //              this is now the only part of the "CompiledShaderByteCode" that is platform specific
        auto* compiler = checked_cast<const D3DShaderCompiler*>(&ShaderService::GetInstance().GetLowLevelCompiler());

        ID3D::ShaderReflection* reflectionTemp = nullptr;
        HRESULT hresult = compiler->D3DReflect_Wrapper(GetByteCode(), GetSize(), __uuidof(ID3D::ShaderReflection), (void**)&reflectionTemp);
        if (!SUCCEEDED(hresult) || !reflectionTemp)
            ThrowException(::Assets::Exceptions::InvalidResource(
                Initializer(), "Error while invoking low-level shader reflection"));
        return moveptr(reflectionTemp);
    }

    const char* CompiledShaderByteCode::Initializer() const
    {
        #if defined(_DEBUG)
            return _initializer;
        #else
            return "";
        #endif
    }

    const uint64 CompiledShaderByteCode::CompileProcessType = ConstHash64<'Shad', 'erCo', 'mpil', 'e'>::Value;

    ShaderService::ResId::ResId(const ResChar filename[], const ResChar entryPoint[], const ResChar shaderModel[])
    {
        XlCopyString(_filename, filename);
        XlCopyString(_entryPoint, entryPoint);
        XlCopyString(_shaderModel, shaderModel);
    }

    ShaderService::ResId::ResId()
    {
        _filename[0] = '\0'; _entryPoint[0] = '\0'; _shaderModel[0] = '\0';
    }

    std::shared_ptr<ShaderService::ILowLevelCompiler> CreateLowLevelShaderCompiler()
    {
        return std::make_shared<D3DShaderCompiler>();
    }
    
}}


