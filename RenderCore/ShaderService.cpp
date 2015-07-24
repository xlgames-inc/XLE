// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderService.h"
#include "Resource.h"
#include "../Assets/Assets.h"
#include "../Assets/IntermediateResources.h"
#include "../Assets/ArchiveCache.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/StringUtils.h"
#include "../Utility/PtrUtils.h"
#include <assert.h>

namespace RenderCore
{

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
                    Throw(Assets::Exceptions::InvalidAsset(Initializer(), ""));
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
                Throw(Assets::Exceptions::PendingAsset(Initializer(), ""));
            
            ResolveFromCompileMarker();
        }

        if (!_shader || _shader->empty()) {
            _shader.reset();
            Throw(Assets::Exceptions::InvalidAsset(Initializer(), ""));
        }
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
            Throw(Assets::Exceptions::InvalidAsset(Initializer(), ""));

        _marker.reset();
    }

    std::pair<const void*, size_t> CompiledShaderByteCode::GetByteCode() const
    {
        Resolve();
        return std::make_pair(
            PtrAdd(AsPointer(_shader->begin()), sizeof(ShaderService::ShaderHeader)),
            _shader->size() - sizeof(ShaderService::ShaderHeader));
    }

    ::Assets::AssetState CompiledShaderByteCode::TryGetByteCode(
        void const*& byteCode, size_t& size)
    {
        if (_shader) {
            byteCode = PtrAdd(AsPointer(_shader->begin()), sizeof(ShaderService::ShaderHeader));
            size = _shader->size() - sizeof(ShaderService::ShaderHeader);
            return ::Assets::AssetState::Ready;
        }

        if (_compileHelper) {
            auto resolveRes = _compileHelper->TryResolve(_shader, Initializer(), _validationCallback);
            if (resolveRes != ::Assets::AssetState::Ready)
                return resolveRes;

            _compileHelper.reset();
        } else if (_marker) {
            auto markerState = _marker->GetState();
            if (markerState != ::Assets::AssetState::Ready)
                return markerState;
            
            ResolveFromCompileMarker();
        }

        if (!_shader || _shader->empty()) {
            _shader.reset();
            return ::Assets::AssetState::Invalid;
        }

        byteCode = PtrAdd(AsPointer(_shader->begin()), sizeof(ShaderService::ShaderHeader));
        size = _shader->size() - sizeof(ShaderService::ShaderHeader);
        return ::Assets::AssetState::Ready;
    }

    bool CompiledShaderByteCode::DynamicLinkingEnabled() const
    {
        if (_stage == ShaderStage::Null) return false;

        Resolve();
        if (_shader->size() < sizeof(ShaderService::ShaderHeader)) return false;
        auto* hdr = (const ShaderService::ShaderHeader*)AsPointer(_shader->begin());
        assert(hdr->_version == ShaderService::ShaderHeader::Version);
        return hdr->_dynamicLinkageEnabled != 0;
    }

    const char* CompiledShaderByteCode::Initializer() const
    {
        #if defined(_DEBUG)
            return _initializer;
        #else
            return "";
        #endif
    }

    CompiledShaderByteCode::CompiledShaderByteCode(CompiledShaderByteCode&& moveFrom)
    : _shader(std::move(moveFrom._shader))
    , _stage(moveFrom._stage)
    , _validationCallback(std::move(moveFrom._validationCallback))
    , _compileHelper(std::move(moveFrom._compileHelper))
    , _marker(std::move(moveFrom._marker))
    {
        #if defined(_DEBUG)
            XlCopyString(_initializer, moveFrom._initializer);
        #endif
    }

    CompiledShaderByteCode& CompiledShaderByteCode::operator=(CompiledShaderByteCode&& moveFrom)
    {
        _shader = std::move(moveFrom._shader);
        _stage = moveFrom._stage;
        _validationCallback = std::move(moveFrom._validationCallback);
        _compileHelper = std::move(moveFrom._compileHelper);
        _marker = std::move(moveFrom._marker);
        #if defined(_DEBUG)
            XlCopyString(_initializer, moveFrom._initializer);
        #endif
        return *this;
    }

    const uint64 CompiledShaderByteCode::CompileProcessType = ConstHash64<'Shad', 'erCo', 'mpil', 'e'>::Value;

        ////////////////////////////////////////////////////////////

    ShaderService::ResId::ResId(const ResChar filename[], const ResChar entryPoint[], const ResChar shaderModel[])
    {
        XlCopyString(_filename, filename);
        XlCopyString(_entryPoint, entryPoint);

        _dynamicLinkageEnabled = shaderModel[0] == '!';
        if (_dynamicLinkageEnabled) ++shaderModel;
        XlCopyString(_shaderModel, shaderModel);
    }

    ShaderService::ResId::ResId()
    {
        _filename[0] = '\0'; _entryPoint[0] = '\0'; _shaderModel[0] = '\0';
        _dynamicLinkageEnabled = false;
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
                if (*(startShaderModel+1) == '!') {
                    shaderId._dynamicLinkageEnabled = true;
                    ++startShaderModel;
                }
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
}

