// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Shader.h"
#include "DeviceContext.h"
#include "../../ShaderService.h"
#include "../../../Assets/IntermediateAssets.h"
#include "../../../Assets/AssetUtils.h"
#include "../../../Assets/InvalidAssetManager.h"
#include "../../../Utility/Streams/PathUtils.h"
#include "../../../Utility/Threading/Mutex.h"
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/StringFormat.h"
#include "../../../Utility/PtrUtils.h"
#include "../../../Utility/Streams/StreamFormatter.h"
#include "../../../Utility/Conversion.h"

#include <regex> // used for parsing parameter definition
#include "../../../Utility/ParameterBox.h"

#include "../../../Utility/WinAPI/WinAPIWrapper.h"
#include "IncludeDX11.h"
#include <D3D11Shader.h>
#include <D3Dcompiler.h>

namespace RenderCore { namespace Metal_DX11
{
    using ::Assets::ResChar;

    static const auto s_shaderReflectionInterfaceGuid = IID_ID3D11ShaderReflection; // __uuidof(ID3D::ShaderReflection); // 

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
            const ShaderService::ResId& shaderPath,
            const ::Assets::ResChar definesTable[]) const;

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

        static D3DShaderCompiler& GetInstance() { return *s_instance; }

    protected:
        mutable Threading::Mutex _moduleLock;
        mutable HMODULE _module;
        HMODULE GetShaderCompileModule() const;

        static D3DShaderCompiler* s_instance;
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
            return 
                  D3D10_SHADER_ENABLE_STRICTNESS 
                | D3D10_SHADER_OPTIMIZATION_LEVEL3 // | D3D10_SHADER_NO_PRESHADER;
                // | D3D10_SHADER_IEEE_STRICTNESS
                // | D3D10_SHADER_WARNINGS_ARE_ERRORS
                ;
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
        ::Assets::ResolvedAssetFile path, buffer;
        for (auto i=_searchDirectories.cbegin(); i!=_searchDirectories.cend(); ++i) {
            XlCopyString(buffer._fn, dimof(buffer._fn), i->c_str());
            XlCatString(buffer._fn, dimof(buffer._fn), pFileName);
            SplitPath<ResChar>(buffer._fn).Simplify().Rebuild(path._fn);

            std::unique_ptr<uint8[]> file;
            ::Assets::DependentFileState timeMarker;
            {
                    // need to use Win32 file operations, so we can get the modification time
                    //  at exactly the time we're reading it.
                auto handle = CreateFile(
                    path._fn, GENERIC_READ, FILE_SHARE_READ,
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
                        return !XlCompareStringI(depState._filename.c_str(), path._fn);
                    });

                if (existing == _includeFiles.cend()) {
                    timeMarker._filename = path._fn;
                    _includeFiles.push_back(timeMarker);
                    
                    auto newDirectory = FileNameSplitter<ResChar>(path._fn).DriveAndPath().AsString();
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
            if (::Assets::Services::GetInvalidAssetMan())
                ::Assets::Services::GetInvalidAssetMan()->MarkInvalid(assetName, errorString);
        }

        static void RegisterValidAsset(const ::Assets::ResChar assetName[])
        {
            if (::Assets::Services::GetInvalidAssetMan())
                ::Assets::Services::GetInvalidAssetMan()->MarkValid(assetName);
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

///////////////////////////////////////////////////////////////////////////////////////////////////

    class FLGFormatter
    {
    public:
        enum class Blob
        {
            Module, Input, Output, Call, Bind,
            ParameterBlock, Assignment,
            Token,
            End
        };
        std::pair<Blob, StringSection<char>> PeekNext();
        void SetPosition(const char* newPosition);

        StreamLocation GetStreamLocation() const { return { unsigned(_iterator - _lineStart), _lineIndex }; }

        FLGFormatter(StringSection<char> script);
        ~FLGFormatter();
    private:
        StringSection<char> _script;
        const char* _iterator;

        unsigned    _lineIndex;
        const char* _lineStart;
    };

    static bool IsWhitespace(char chr)          { return chr == ' ' || chr == '\t'; }
    static bool IsNewlineWhitespace(char chr)   { return chr == '\r' || chr == '\n'; }
    static bool IsIgnoreable(char chr)          { return chr == ')'; }

    auto FLGFormatter::PeekNext() -> std::pair<Blob, StringSection<char>>
    {
        while (_iterator < _script.end()) {
            if (IsWhitespace(*_iterator) || IsIgnoreable(*_iterator)) {
                ++_iterator;
            } else if (*_iterator == '\n' || *_iterator == '\r') {
                if (*_iterator == '\r' && (_iterator+1) < _script.end() && *(_iterator+1) == '\n')
                    ++_iterator;
                ++_iterator;
                ++_lineIndex;
                _lineStart = _iterator;
            } else 
                break;
        }

        if (_iterator == _script.end())
            return std::make_pair(Blob::End, StringSection<char>());

        // check for known tokens -- 
        if (*_iterator == '=') {
            return std::make_pair(Blob::Assignment, StringSection<char>(_iterator, _iterator+1));
        } else if (*_iterator == '(') {
                // This is a parameter block. We need to scan forward over everything until
                // we reach the end bracket. The end bracket is the only thing we care about. Everything
                // else gets collapsed into the parameter block.
            const auto* i = _iterator+1;
            for (;;) {
                if (i == _script.end())
                    Throw(FormatException(
                        "Missing closing ')' on parameter block",
                        GetStreamLocation()));
                if (*i == ')') break;
                ++i;
            }
            return std::make_pair(Blob::ParameterBlock, StringSection<char>(_iterator+1, i));
        } else {
            static const std::pair<Blob, StringSection<char>> KnownTokens[] = 
            {
                std::make_pair(Blob::Module,    "Module"),
                std::make_pair(Blob::Input,     "Input"),
                std::make_pair(Blob::Output,    "Output"),
                std::make_pair(Blob::Call,      "Call"),
                std::make_pair(Blob::Bind,      "Bind")
            };
            // read forward to any token terminator
            const char* i = _iterator;
            while (i < _script.end() && !IsWhitespace(*i) && !IsIgnoreable(*i) && *i != '\r' && *i != '\n')
                ++i;

            auto token = MakeStringSection(_iterator, i);
            for (unsigned c=0; c<dimof(KnownTokens); ++c)
                if (XlEqString(token, KnownTokens[c].second))
                    return std::make_pair(KnownTokens[c].first, token);
                
            return std::make_pair(Blob::Token, token);
        }
    }

    void FLGFormatter::SetPosition(const char* newPosition)
    {
        // typically called after PeekNext(), we should advance to the given
        // position. While advancing, we have to look for new lines!
        assert(newPosition >= _iterator && newPostion <= _script.end());
        while (_iterator < newPosition) {
            if (*_iterator == '\n' || *_iterator == '\r') {
                    // note that if we attempt to "SetPosition" to a point in the middle of "\r\n"
                    // we will automatically get adjusted to after the \n
                if (*_iterator == '\r' && (_iterator+1) < _script.end() && *(_iterator+1) == '\n')
                    ++_iterator;
                ++_lineIndex;
                _lineStart = _iterator;
            }
            ++_iterator;
        }
    }

    FLGFormatter::FLGFormatter(StringSection<char> script)
    : _script(script)
    {
        _lineIndex = 1;
        _lineStart = _script.begin();
    }

    FLGFormatter::~FLGFormatter() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class FunctionLinkingModule
    {
    public:
        ID3D11Module* GetUnderlying() { return _module.get(); }

        FunctionLinkingModule(const ::Assets::ResChar initializer[]);
        ~FunctionLinkingModule();
    private:
        intrusive_ptr<ID3D11Module> _module;
    };

    FunctionLinkingModule::FunctionLinkingModule(const ::Assets::ResChar initializer[])
    {
        auto& byteCode = ::Assets::GetAssetDep<CompiledShaderByteCode>(initializer);
        auto state = byteCode.StallWhilePending();
        if (state != ::Assets::AssetState::Ready)
            Throw(::Assets::Exceptions::InvalidAsset(initializer, "Shader compile failure while building function linking module"));
        auto code = byteCode.GetByteCode();

        ID3D11Module* rawModule = nullptr;
        auto hresult = D3DLoadModule(code.first, code.second, &rawModule);
        _module = moveptr(rawModule);
        if (!SUCCEEDED(hresult))
            Throw(::Assets::Exceptions::InvalidAsset(initializer, "Failure while creating shader module from compiled shader byte code"));
    }

    FunctionLinkingModule::~FunctionLinkingModule() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename First, typename Second>
        class StringCompareFirst
        {
        public:
            inline bool operator()(const std::pair<First, Second>& lhs, const std::pair<First, Second>& rhs) const   { return XlCompareString(lhs.first, rhs.first) < 0; }
            inline bool operator()(const std::pair<First, Second>& lhs, const First& rhs) const                      { return XlCompareString(lhs.first, rhs) < 0; }
            inline bool operator()(const First& lhs, const std::pair<First, Second>& rhs) const                      { return XlCompareString(lhs, rhs.first) < 0; }
        };

    template<typename Object>
        typename std::vector<std::pair<StringSection<char>, intrusive_ptr<Object>>>::iterator
            LowerBoundT(
                std::vector<std::pair<StringSection<char>, intrusive_ptr<Object>>>& vector,
                StringSection<char> comparison)
        {
            return std::lower_bound(
                vector.begin(), vector.end(), comparison,
                StringCompareFirst<StringSection<char>, intrusive_ptr<Object>>());
        }

    class FunctionLinkingGraph
    {
    public:

        using Section = StringSection<char>;
        FunctionLinkingGraph(Section script);
        ~FunctionLinkingGraph();
    private:
        void ParseAssignmentExpression(FLGFormatter& formatter, Section variableName);

        intrusive_ptr<ID3D11FunctionLinkingGraph> _graph;

        using ModulePtr = intrusive_ptr<ID3D11Module>;
        using NodePtr = intrusive_ptr<ID3D11LinkingNode>;
        std::vector<std::pair<Section, ModulePtr>>  _modules;
        std::vector<std::pair<Section, NodePtr>>    _nodes;
    };

    FunctionLinkingGraph::FunctionLinkingGraph(StringSection<char> script)
    {
        ID3D11FunctionLinkingGraph* graphRaw = nullptr;
        auto hresult = D3DCreateFunctionLinkingGraph(0, &graphRaw);
        if (!SUCCEEDED(hresult))
            Throw(::Exceptions::BasicLabel::BasicLabel("Failure while creating D3D function linking graph"));
        _graph = moveptr(graphRaw);

        using Blob = FLGFormatter::Blob;
        FLGFormatter formatter(script);
        for (;;) {
            auto next = formatter.PeekNext();
            if (next.first == Blob::End) break;
            
            switch (next.first) {
            case Blob::Token:
                {
                        // expecting an '=' token after this
                    formatter.SetPosition(next.second.end());
                    auto expectingAssignment = formatter.PeekNext();
                    if (expectingAssignment.first != Blob::Assignment)
                        Throw(FormatException("Expecting assignment after variable name", formatter.GetStreamLocation()));
                    formatter.SetPosition(expectingAssignment.second.end());

                    ParseAssignmentExpression(formatter, next.second);
                    break;
                }
            }
        }
    }

    FunctionLinkingGraph::~FunctionLinkingGraph() {}

    class ShaderParameter
    {
    public:
        std::string _name, _semanticName;
        D3D_SHADER_VARIABLE_TYPE _type;
        D3D_SHADER_VARIABLE_CLASS _class;
        unsigned _rows, _columns;

        D3D11_PARAMETER_DESC AsParameterDesc()
        {
            return 
              { _name.c_str(), _semanticName.c_str(),
                _type, _class, _rows, _columns,
                D3D_INTERPOLATION_UNDEFINED, D3D_PF_NONE,
                0, 0, 0, 0 };
        }

        ShaderParameter(StringSection<char> param);
    };

    static std::regex ShaderParameterParse(R"--((\w+)\s+(\w+)\s*(?:\:\s*(\w+))\s*)--");

    template<typename DestType = unsigned, typename CharType = char>
        DestType StringToUnsigned(const StringSection<CharType> source)
    {
        auto* start = source.begin();
        auto* end = source.end();
        if (start >= end) return 0;

        auto result = DestType(0);
        for (;;) {
            if (start >= end) break;
            if (*start < '0' || *start > '9') break;
            result = (result * 10) + DestType((*start) - '0');
            ++start;
        }
        return result;
    }

    static ImpliedTyping::TypeDesc HLSLTypeNameAsTypeDesc(StringSection<char> hlslTypeName)
    {
        using namespace ImpliedTyping;
        static std::pair<StringSection<char>, ImpliedTyping::TypeCat> baseTypes[] = 
        {
            { "float", TypeCat::Float },
            { "uint", TypeCat::UInt32 },
            { "dword", TypeCat::UInt32 },
            { "int", TypeCat::Int32 },
            { "byte", TypeCat::UInt8 }
            // "half", "double" not supported
        };
        for (unsigned c=0; c<dimof(baseTypes); ++c) {
            auto len = baseTypes[c].first.Length();
            if (hlslTypeName.Length() >= len
                && !XlComparePrefix(baseTypes[c].first.begin(), hlslTypeName.begin(), len)) {

                const auto matrixMarker = XlFindChar(&hlslTypeName[len], 'x');
                if (matrixMarker != nullptr) {
                    auto count0 = StringToUnsigned(MakeStringSection(&hlslTypeName[len], matrixMarker));
                    auto count1 = StringToUnsigned(MakeStringSection(matrixMarker+1, hlslTypeName.end()));

                    TypeDesc result;
                    result._arrayCount = (uint16)std::max(1u, count0 * count1);
                    result._type = baseTypes[c].second;
                    result._typeHint = TypeHint::Matrix;
                    return result;
                } else {
                    auto count = StringToUnsigned(MakeStringSection(&hlslTypeName[len], hlslTypeName.end()));
                    if (count == 0 || count > 4) count = 1;
                    TypeDesc result;
                    result._arrayCount = (uint16)count;
                    result._type = baseTypes[c].second;
                    result._typeHint = (count > 1) ? TypeHint::Vector : TypeHint::None;
                    return result;
                }
            }
        }

        return TypeDesc();
    }

    ShaderParameter::ShaderParameter(StringSection<char> param)
    {
        // Our parameters are always of the format "type name [: semantic]"
        // We ignore some other HLSL syntax elements (in/out/inout and interpolation modes)
        // The parse is simple, but let's use regex anwyay

        _type = D3D_SVT_VOID;
        _class = D3D_SVC_SCALAR;
        _rows = _columns = 1;

        std::match_results<const char*> match;
        bool a = std::regex_match(param.begin(), param.end(), match, ShaderParameterParse);
        if (a && match.size() >= 3) {
            _name = std::string(match[2].first, match[2].second);
            if (match.size() >= 4)
                _semanticName = std::string(match[3].first, match[3].second);

            auto typeName = MakeStringSection(match[1].first, match[1].second);
            auto typeDesc = HLSLTypeNameAsTypeDesc(typeName);
            
                // Convert the "typeDesc" values into the types used by the HLSL library
            switch (typeDesc._type) {
            case ImpliedTyping::TypeCat::Float:     _type = D3D_SVT_FLOAT; break;
            case ImpliedTyping::TypeCat::UInt32:    _type = D3D_SVT_UINT; break;
            case ImpliedTyping::TypeCat::Int32:     _type = D3D_SVT_INT; break;
            case ImpliedTyping::TypeCat::UInt8:     _type = D3D_SVT_UINT8; break;
            default:
                Throw(::Exceptions::BasicLabel("Unknown parameter type encountered in ShaderParameter (%s)", typeName.AsString().c_str()));
                break;
            }

            _class = (typeDesc._arrayCount <= 1) ? D3D_SVC_VECTOR : D3D_SVC_SCALAR;
            _rows = typeDesc._arrayCount;
            _columns = 1;
        }
    }

    static std::vector<ShaderParameter> ParseParameters(StringSection<char> input)
    {
        // This should be a comma separated list of parameterss
        // just have to do the separation by comma here...
        std::vector<ShaderParameter> result;
        for (;;) {
            const auto* i = input.begin();
            while (i < input.end() && (IsWhitespace(*i) || IsNewlineWhitespace(*i))) ++i;

            if (i == input.end()) break;

            const auto* start = i;
            while (i < input.end() && *i != ',') ++i;
            result.emplace_back(ShaderParameter(MakeStringSection(start, i)));
            if (i == input.end()) break;
            ++i;
        }

        return std::move(result);
    }

    void FunctionLinkingGraph::ParseAssignmentExpression(FLGFormatter& formatter, Section variableName)
    {
        auto startLoc = formatter.GetStreamLocation();
        
        using Blob = FLGFormatter::Blob;
        auto next = formatter.PeekNext();
        if (   next.first != Blob::Module && next.first != Blob::Input
            && next.first != Blob::Output && next.first != Blob::Call)
            Throw(FormatException("Unexpected token after assignment operation", formatter.GetStreamLocation()));
        formatter.SetPosition(next.second.end());

        // parse the parameter block that comes next
        auto paramBlockLoc = formatter.GetStreamLocation();
        auto p = formatter.PeekNext();
        if (p.first != Blob::ParameterBlock)
            Throw(FormatException("Expecting parameter block", formatter.GetStreamLocation()));
        formatter.SetPosition(p.second.end());
        auto parameterBlock = p.second;

        // we must perform some operations based on the token we got...
        switch (next.first) {
        case Blob::Module:
            {
                    // This is an operation to construct a new module instance. We must load the
                    // module from some other source file, and then construct a module instance from it.
                    // Note that we can do relative paths for this module name reference here -- 
                    //      it must be an full asset path
                    // Also, we should explicitly specify the shader version number and use the "lib" type
                FunctionLinkingModule module(
                    Conversion::Convert<::Assets::rstring>(parameterBlock.AsString()).c_str());
                // ID3D11ModuleInstance* moduleInstanceRaw = nullptr;
                // auto hresult = module.GetUnderlying()->CreateInstance(nullptr, &moduleInstanceRaw);
                // intrusive_ptr<ID3D11ModuleInstance> moduleInstance = moveptr(moduleInstanceRaw);
                // if (!SUCCEEDED(hresult))
                //     Throw(::Exceptions::BasicLabel("D3D error while creating module instance from shader module"));

                auto i = LowerBoundT(_modules, variableName);
                if (i != _modules.end() && XlEqString(i->first, variableName))
                    Throw(FormatException("Attempting to reassign module that is already assigned. Check for naming conflicts.", startLoc));

                _modules.insert(i, std::make_pair(variableName, module.GetUnderlying()));
            }
            break;

        case Blob::Input:
        case Blob::Output:
            {
                // For both input and output, our parameter block should be a list of 
                // parameters (in a HLSL-like syntax). We use this to create a linking
                // node (which can be bound using the Bind instruction).
                auto params = ParseParameters(parameterBlock);

                std::vector<D3D11_PARAMETER_DESC> finalParams;
                finalParams.reserve(params.size());
                for (auto&i:params) finalParams.push_back(i.AsParameterDesc());

                ID3D11LinkingNode* linkingNodeRaw = nullptr;
                HRESULT hresult;
                if (next.first == Blob::Input) {
                    hresult = _graph->SetInputSignature(AsPointer(finalParams.cbegin()), (unsigned)finalParams.size(), &linkingNodeRaw);
                } else {
                    hresult = _graph->SetOutputSignature(AsPointer(finalParams.cbegin()), (unsigned)finalParams.size(), &linkingNodeRaw);
                }
                intrusive_ptr<ID3D11LinkingNode> linkingNode = moveptr(linkingNodeRaw);
                if (!SUCCEEDED(hresult))
                    Throw(::Exceptions::BasicLabel("D3D error while creating input or output linking node"));

                auto i = LowerBoundT(_nodes, variableName);
                if (i != _nodes.end() && XlEqString(i->first, variableName))
                    Throw(FormatException("Attempting to reassign node that is already assigned. Check for naming conflicts.", startLoc));

                _nodes.insert(i, std::make_pair(variableName, std::move(linkingNode)));
            }
            break;

        case Blob::Call:
            {
                // Our parameters should be a function name with the form:
                //      <<ModuleName>>.<<FunctionName>>
                // The module name should correspond to a module previously loaded
                // and assigned with the Module instruction. This will create a new
                // linking node.

                auto* i = parameterBlock.begin();
                while (i != parameterBlock.end() && *i != '.') ++i;

                if (i == parameterBlock.end())
                    Throw(FormatException("Expected a module and function name in Call instruction.", paramBlockLoc));

                auto moduleName = MakeStringSection(parameterBlock.begin(), i);
                auto m = LowerBoundT(_modules, moduleName);
                if (m == _modules.end() || !XlEqString(m->first, moduleName))
                    Throw(FormatException("Unknown module variable in Call instruction. Modules should be registered with Module instruction before using.", paramBlockLoc));

                auto module = m->second.get();

                ID3D11LinkingNode* linkingNodeRaw = nullptr;
                auto hresult = _graph->CallFunction(nullptr, module, 
                    MakeStringSection(i+1, parameterBlock.end()).AsString().c_str(),
                    &linkingNodeRaw);
                intrusive_ptr<ID3D11LinkingNode> linkingNode = moveptr(linkingNodeRaw);
                if (!SUCCEEDED(hresult))
                    Throw(FormatException("D3D error while creating linking node for function call. Requested function may be missing from the module.", paramBlockLoc));

                auto n = LowerBoundT(_nodes, variableName);
                if (n != _nodes.end() && XlEqString(n->first, variableName))
                    Throw(FormatException("Attempting to reassign node that is already assigned. Check for naming conflicts.", startLoc));

                _nodes.insert(n, std::make_pair(variableName, std::move(linkingNode)));
            }
            break;
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    bool D3DShaderCompiler::DoLowLevelCompile(
        /*out*/ std::shared_ptr<std::vector<uint8>>& payload,
        /*out*/ std::shared_ptr<std::vector<uint8>>& errors,
        /*out*/ std::vector<::Assets::DependentFileState>& dependencies,
        const void* sourceCode, size_t sourceCodeLength,
        const ShaderService::ResId& shaderPath,
        const ::Assets::ResChar definesTable[]) const
    {
            // This is called (typically in a background thread)
            // after the shader data has been loaded from disk.
            // Here we will invoke the D3D compiler. It will block
            // in this thread (so we should normally only call this from
            // a background thread (often one in a thread pool)
        ID3DBlob* codeResult = nullptr, *errorResult = nullptr;

        std::string definesCopy;
        auto arrayOfDefines = MakeDefinesTable(definesTable, shaderPath._shaderModel, definesCopy);

            // Check for the marker labelling this as a function linking graph shader
        const char fileId[] = "FunctionLinkingGraph";
        StringSection<char> asStringSection(
            (const char*)sourceCode, 
            (const char*)PtrAdd(sourceCode, std::min(sourceCodeLength, XlStringLen(fileId))));
        if (XlEqString(asStringSection, fileId)) {

                // This a FunctionLinkingGraph definition
                // look for the version number, and then parse this file as
                // a simple script language.
            const char* i = (const char*)PtrAdd(sourceCode, XlStringLen(fileId));
            const char* e = (const char*)PtrAdd(sourceCode, sourceCodeLength);

                // A version number may follow -- but it's optional
            if (i < e && *i == ':') {
                ++i;
                auto versionStart = i;
                while (i < e && *i != '\n' && *i != '\r') ++i;
                StringSection<char> version(versionStart, i);
                char temp[32]; const char* endPtr;
                XlCopyString(temp, version);
                auto versionNumber = XlAtoI32(temp, &endPtr);
                if (versionNumber != 0 || endPtr != version.end())
                    Throw(::Exceptions::BasicLabel("Function linking graph script version unsupported (%s)", version.AsString().c_str()));
            }

                // Now we scan through, token at a time... And as we go, we will build a 
                // ID3DFunctionLinkingGraph.
                // Our scripting language as a pretty simple 1:1 mapping to the D3D functions.

        } else {

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
                payload = std::make_shared<std::vector<uint8>>(codeResult->GetBufferSize() + sizeof(ShaderService::ShaderHeader));
                auto* hdr = (ShaderService::ShaderHeader*)AsPointer(payload->begin());
                hdr->_version = ShaderService::ShaderHeader::Version;
                hdr->_dynamicLinkageEnabled = shaderPath._dynamicLinkageEnabled;
                XlCopyMemory(
                    PtrAdd(AsPointer(payload->begin()), sizeof(ShaderService::ShaderHeader)),
                    (uint8*)codeResult->GetBufferPointer(), codeResult->GetBufferSize());
            }

            errors.reset();
            if (errorResult && errorResult->GetBufferPointer() && errorResult->GetBufferSize()) {
                errors = std::make_shared<std::vector<uint8>>(
                    (uint8*)errorResult->GetBufferPointer(), 
                    PtrAdd((uint8*)errorResult->GetBufferPointer(), errorResult->GetBufferSize()));
            }

            dependencies = includeHandler.GetIncludeFiles();
            dependencies.push_back(
                ::Assets::IntermediateAssets::Store::GetDependentFileState(shaderPath._filename));   // also need a dependency for the base file

            if (SUCCEEDED(hresult)) { MarkValid(shaderPath); }
            else                    { MarkInvalid(shaderPath, hresult, errors); }

            return SUCCEEDED(hresult);

        }
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
        auto hresult = D3DReflect_Wrapper(data, dataSize, s_shaderReflectionInterfaceGuid, (void**)&reflTemp);
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

    D3DShaderCompiler* D3DShaderCompiler::s_instance = nullptr;

    D3DShaderCompiler::D3DShaderCompiler() 
    {
        _module = (HMODULE)INVALID_HANDLE_VALUE;

        assert(s_instance == nullptr);
        s_instance = this;
    }

    D3DShaderCompiler::~D3DShaderCompiler()
    {
        assert(s_instance == this);
        s_instance = nullptr;

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

    intrusive_ptr<ID3D::ShaderReflection> CreateReflection(const CompiledShaderByteCode& shaderCode)
    {
        auto stage = shaderCode.GetStage();
        if (stage == ShaderStage::Null)
            return intrusive_ptr<ID3D::ShaderReflection>();

            // awkward -- we have to use a singleton pattern to get access to the compiler here...
            //          Otherwise, we could potentially have multiple instances of D3DShaderCompiler.
        auto& compiler = D3DShaderCompiler::GetInstance();

        auto byteCode = shaderCode.GetByteCode();

        ID3D::ShaderReflection* reflectionTemp = nullptr;
        HRESULT hresult = compiler.D3DReflect_Wrapper(byteCode.first, byteCode.second, s_shaderReflectionInterfaceGuid, (void**)&reflectionTemp);
        if (!SUCCEEDED(hresult) || !reflectionTemp)
            Throw(::Assets::Exceptions::InvalidAsset(
                shaderCode.Initializer(), "Error while invoking low-level shader reflection"));
        return moveptr(reflectionTemp);
    }

    std::shared_ptr<ShaderService::ILowLevelCompiler> CreateLowLevelShaderCompiler()
    {
        return std::make_shared<D3DShaderCompiler>();
    }

}}


