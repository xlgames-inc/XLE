// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderService.h"
#include "Types.h"	// For PS_DefShaderModel
#include "../Assets/Assets.h"
#include "../Assets/IntermediateAssets.h"
#include "../Assets/ArchiveCache.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/StringUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/Streams/PathUtils.h"
#include <assert.h>

namespace RenderCore
{

        ////////////////////////////////////////////////////////////

    static ShaderStage::Enum AsShaderStage(StringSection<::Assets::ResChar> shaderModel)
    {
		assert(shaderModel.size() >= 1);
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

    ShaderStage::Enum ShaderService::ResId::AsShaderStage() const { return RenderCore::AsShaderStage(_shaderModel); }

    CompiledShaderByteCode::CompiledShaderByteCode(std::shared_ptr<::Assets::ICompileMarker>&& marker)
    {
            // no way to know the shader stage in this mode...
            //  Maybe the shader stage should be encoded in the intermediate file name
        _stage = ShaderStage::Null;
        #if defined(STORE_SHADER_INITIALIZER)
            _initializer[0] = '\0';
        #endif

        if (marker) {
            #if defined(STORE_SHADER_INITIALIZER)
                XlCopyString(_initializer, marker->Initializer());
            #endif
            auto existing = marker->GetExistingAsset();

            auto i = strrchr(existing._sourceID0, '-');
            if (i) _stage = AsShaderStage(i+1);

            if (existing._dependencyValidation && existing._dependencyValidation->GetValidationIndex() == 0) {
                _shader = existing._archive->TryOpenFromCache(existing._sourceID1);
                if (_shader)
                    _validationCallback = std::move(existing._dependencyValidation);
            } 

            if (!_shader) {
                _validationCallback = std::make_shared<Assets::DependencyValidation>();
                _marker = marker->InvokeCompile();
            }
        } else
            _validationCallback = std::make_shared<Assets::DependencyValidation>();
    }

    CompiledShaderByteCode::CompiledShaderByteCode(StringSection<::Assets::ResChar> initializer, StringSection<::Assets::ResChar> definesTable)
    {
        _stage = ShaderStage::Null;
        auto validationCallback = std::make_shared<Assets::DependencyValidation>();
        std::shared_ptr<ShaderService::IPendingMarker> compileHelper;
        #if defined(STORE_SHADER_INITIALIZER)
            XlCopyString(_initializer, initializer);
        #endif

        if (!initializer.IsEmpty() && !XlEqStringI(initializer, "null")) {
            compileHelper = 
                ShaderService::GetInstance().CompileFromFile(
                    initializer, definesTable);

            if (compileHelper != nullptr)
                _stage = compileHelper->GetStage();

            RegisterFileDependency(validationCallback, MakeFileNameSplitter(initializer).AllExceptParameters());
        }

        _validationCallback = std::move(validationCallback);
        _compileHelper = std::move(compileHelper);
    }

    CompiledShaderByteCode::CompiledShaderByteCode(
        StringSection<char> shaderInMemory, StringSection<char> entryPoint, 
        StringSection<char> shaderModel, StringSection<::Assets::ResChar> definesTable)
    {
        _stage = AsShaderStage(shaderModel);
        auto validationCallback = std::make_shared<Assets::DependencyValidation>();
        #if defined(STORE_SHADER_INITIALIZER)
            XlCopyString(_initializer, "ShaderInMemory");
        #endif
        auto compileHelper = 
            ShaderService::GetInstance().CompileFromMemory(
                shaderInMemory, entryPoint, shaderModel, definesTable);

        _validationCallback = std::move(validationCallback);
        _compileHelper = std::move(compileHelper);
    }

    CompiledShaderByteCode::CompiledShaderByteCode(std::shared_ptr<ShaderService::IPendingMarker>&& marker)
    {
        #if defined(STORE_SHADER_INITIALIZER)
            XlCopyString(_initializer, "ManualShader");
        #endif
        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        _stage = marker->GetStage();
        _compileHelper = std::move(marker);
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
            if (_marker->GetAssetState() == ::Assets::AssetState::Pending)
                Throw(Assets::Exceptions::PendingAsset(Initializer(), "Marker is still pending while resolving shader code"));
            
            ResolveFromCompileMarker();
        }

        if (!_shader || _shader->empty()) {
            _shader.reset();
            Throw(Assets::Exceptions::InvalidAsset(Initializer(), "CompiledShaderByteCode invalid"));
        }
    }

    void CompiledShaderByteCode::ResolveFromCompileMarker() const
    {
        if (_marker->GetAssetState() != ::Assets::AssetState::Invalid) {
            auto loc = _marker->GetLocator();

                //  Our shader should be stored in a shader cache file
                //  Find that file, and get the completed shader.
                //  Note that this might hit the disk currently...?
            if (loc._archive) {
                _shader = loc._archive->TryOpenFromCache(loc._sourceID1);
                if (!_shader) {
                    LogWarning << "Compilation marker is finished, but shader couldn't be opened from cache (" << loc._sourceID0 << ":" <<loc._sourceID1 << ")";
                    // caller should throw InvalidAsset in this case
                }
            }
        }

            //  Even when we're considered invalid, we must register a dependency, and release
            //  the marker. This way we will be recompiled if the asset is changed (eg, to fix
            //  the compile error)
        if (_marker->GetLocator()._dependencyValidation)
            Assets::RegisterAssetDependency(_validationCallback, _marker->GetLocator()._dependencyValidation);

        if (_marker->GetAssetState() == ::Assets::AssetState::Invalid)
            Throw(Assets::Exceptions::InvalidAsset(Initializer(), "CompiledShaderByteCode invalid"));

        _marker.reset();
    }

    std::pair<const void*, size_t> CompiledShaderByteCode::GetByteCode() const
    {
        Resolve();
        return std::make_pair(
            PtrAdd(AsPointer(_shader->begin()), sizeof(ShaderService::ShaderHeader)),
            _shader->size() - sizeof(ShaderService::ShaderHeader));
    }

    ::Assets::AssetState CompiledShaderByteCode::GetAssetState() const
    {
        if (_shader)
            return ::Assets::AssetState::Ready;

        if (_compileHelper) {
            auto resolveRes = _compileHelper->TryResolve(_shader, _validationCallback);
            if (resolveRes != ::Assets::AssetState::Ready)
                return resolveRes;

            _compileHelper.reset();
        } else if (_marker) {
            auto markerState = _marker->GetAssetState();
            if (markerState != ::Assets::AssetState::Ready)
                return markerState;
            
            ResolveFromCompileMarker();
        }

        if (!_shader || _shader->empty()) {
            _shader.reset();
            return ::Assets::AssetState::Invalid;
        }

        return ::Assets::AssetState::Ready;
    }

    ::Assets::AssetState CompiledShaderByteCode::TryGetByteCode(
        void const*& byteCode, size_t& size)
    {
        auto state = GetAssetState();
        if (state != ::Assets::AssetState::Ready) return state;

        assert(_shader && !_shader->empty());
        byteCode = PtrAdd(AsPointer(_shader->begin()), sizeof(ShaderService::ShaderHeader));
        size = _shader->size() - sizeof(ShaderService::ShaderHeader);
        return ::Assets::AssetState::Ready;
    }

    std::shared_ptr<std::vector<uint8>> CompiledShaderByteCode::GetErrors() const
    {
        if (_compileHelper)
            return _compileHelper->GetErrors();
        return std::shared_ptr<std::vector<uint8>>();
    }

    ::Assets::AssetState CompiledShaderByteCode::StallWhilePending() const
    {
        if (_shader)
            return ::Assets::AssetState::Ready;

        if (_compileHelper) {
            return _compileHelper->StallWhilePending();
        } else if (_marker) {
            return _marker->StallWhilePending();
        }

        return ::Assets::AssetState::Invalid;
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
        #if defined(STORE_SHADER_INITIALIZER)
            return _initializer;
        #else
            return "<<unknown shader>>";
        #endif
    }

    CompiledShaderByteCode::CompiledShaderByteCode(CompiledShaderByteCode&& moveFrom)
    : _shader(std::move(moveFrom._shader))
    , _stage(moveFrom._stage)
    , _validationCallback(std::move(moveFrom._validationCallback))
    , _compileHelper(std::move(moveFrom._compileHelper))
    , _marker(std::move(moveFrom._marker))
    {
        #if defined(STORE_SHADER_INITIALIZER)
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
        #if defined(STORE_SHADER_INITIALIZER)
            XlCopyString(_initializer, moveFrom._initializer);
        #endif
        return *this;
    }

    const uint64 CompiledShaderByteCode::CompileProcessType = ConstHash64<'Shad', 'erCo', 'mpil', 'e'>::Value;

        ////////////////////////////////////////////////////////////

    ShaderService::ResId::ResId(StringSection<ResChar> filename, StringSection<ResChar> entryPoint, StringSection<ResChar> shaderModel)
    {
        XlCopyString(_filename, filename);
        XlCopyString(_entryPoint, entryPoint);

        _dynamicLinkageEnabled = shaderModel[0] == '!';
        if (_dynamicLinkageEnabled) {
			XlCopyString(_shaderModel, MakeStringSection(shaderModel.begin()+1, shaderModel.end()));
		} else {
			XlCopyString(_shaderModel, shaderModel);
		}
    }

    ShaderService::ResId::ResId()
    {
        _filename[0] = '\0'; _entryPoint[0] = '\0'; _shaderModel[0] = '\0';
        _dynamicLinkageEnabled = false;
    }


    auto ShaderService::MakeResId(
        StringSection<::Assets::ResChar> initializer,
        ILowLevelCompiler& compiler) -> ResId
    {
        ResId shaderId;

        const ::Assets::ResChar* startShaderModel = nullptr;
        auto splitter = MakeFileNameSplitter(initializer);
        XlCopyString(shaderId._filename, splitter.AllExceptParameters());

        if (splitter.Parameters().IsEmpty()) {
            XlCopyString(shaderId._entryPoint, "main");
        } else {
            startShaderModel = XlFindChar(splitter.Parameters().begin(), ':');

            if (!startShaderModel) {
                XlCopyString(shaderId._entryPoint, splitter.Parameters().begin());
            } else {
                XlCopyNString(shaderId._entryPoint, splitter.Parameters().begin(), startShaderModel - splitter.Parameters().begin());
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
        compiler.AdaptShaderModel(shaderId._shaderModel, dimof(shaderId._shaderModel), shaderId._shaderModel);

        return std::move(shaderId);
    }

    auto ShaderService::CompileFromFile(
        StringSection<::Assets::ResChar> resId, 
        StringSection<::Assets::ResChar> definesTable) const -> std::shared_ptr<IPendingMarker>
    {
        for (const auto& i:_shaderSources) {
            auto r = i->CompileFromFile(resId, definesTable);
            if (r) return r;
        }
        return nullptr;
    }

    auto ShaderService::CompileFromMemory(
        StringSection<char> shaderInMemory, 
        StringSection<char> entryPoint, StringSection<char> shaderModel, 
        StringSection<::Assets::ResChar> definesTable) const -> std::shared_ptr<IPendingMarker>
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

