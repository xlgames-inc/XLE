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
#include "../Assets/DeferredConstruction.h"
/*#if defined(HAS_XLE_CONSOLE_RIG)
    #include "../ConsoleRig/Log.h"
#endif*/
#include "../Utility/StringUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/Streams/PathUtils.h"
#include <assert.h>

namespace RenderCore
{

        ////////////////////////////////////////////////////////////

    static ShaderStage AsShaderStage(StringSection<::Assets::ResChar> shaderModel)
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

    ShaderStage ShaderService::ResId::AsShaderStage() const { return RenderCore::AsShaderStage(_shaderModel); }

    CompiledShaderByteCode::CompiledShaderByteCode(const std::shared_ptr<::Assets::DeferredConstruction>& construction)
	: _deferredConstructor(construction)
    {
        _stage = ShaderStage::Null;
        #if defined(STORE_SHADER_INITIALIZER)
            _initializer[0] = '\0';
        #endif

		if (_deferredConstructor) {
			_validationCallback = _deferredConstructor->GetDependencyValidation();
			auto shaderStageFn = ConstHash64<'Shad','erSt','age'>::Value;
			if (_deferredConstructor->GetVariants().Has<ShaderStage()>(shaderStageFn))
				_stage = _deferredConstructor->GetVariants().CallDefault<>(shaderStageFn, _stage);
			assert(_stage != ShaderStage::Null);
		} else
			_validationCallback = std::make_shared<Assets::DependencyValidation>();
    }

	CompiledShaderByteCode::CompiledShaderByteCode(const ::Assets::IArtifact& locator, ShaderStage stage, StringSection<::Assets::ResChar> initializer)
	: _shader(locator.GetBlob())
	, _validationCallback(locator.GetDependencyValidation())
	, _stage(stage)
	{
		assert(_validationCallback);
        #if defined(STORE_SHADER_INITIALIZER)
			XlCopyString(_initializer, initializer);
        #endif
	}

	CompiledShaderByteCode::CompiledShaderByteCode(const std::shared_ptr<std::vector<uint8>>& shader, ShaderStage stage, const ::Assets::DepValPtr& depVal) 
	: _shader(shader)
	, _validationCallback(depVal)
	, _stage(stage)
	{
		#if defined(STORE_SHADER_INITIALIZER)
			_initializer[0] = '\0';
		#endif
	}

	CompiledShaderByteCode::CompiledShaderByteCode()
	{
		_validationCallback = std::make_shared<::Assets::DependencyValidation>();
		#if defined(STORE_SHADER_INITIALIZER)
			_initializer[0] = '\0';
		#endif
	}

	std::shared_ptr<::Assets::DeferredConstruction> CompiledShaderByteCode::BeginDeferredConstruction(
		StringSection<char> shaderInMemory, StringSection<char> entryPoint, 
		StringSection<char> shaderModel, StringSection<::Assets::ResChar> definesTable)
	{
		return nullptr;
	}

	std::shared_ptr<::Assets::DeferredConstruction> CompiledShaderByteCode::BeginDeferredConstruction(
		StringSection<::Assets::ResChar> initializer, 
		StringSection<::Assets::ResChar> definesTable)
    {
		const StringSection<::Assets::ResChar> initializers[] = { initializer, definesTable };
		auto marker = ::Assets::Internal::BeginCompileOperation(
			::Assets::GetCompileProcessType<CompiledShaderByteCode>(), 
			initializers, dimof(initializers));
		if (!marker) return nullptr;

		auto existingAsset = marker->GetExistingAsset();
        auto stage = ShaderService::MakeResId(initializer).AsShaderStage();

        auto existingDepVal = existingAsset->GetDependencyValidation();
		if (existingDepVal && existingDepVal->GetValidationIndex()==0) {

			std::unique_ptr<CompiledShaderByteCode> asset = nullptr;
			TRY {
				asset = ::Assets::Internal::ConstructFromIntermediateAssetLocator<CompiledShaderByteCode>(*existingAsset, stage, initializer);
			} CATCH (const ::Assets::Exceptions::InvalidAsset&) {
			} CATCH (const ::Assets::Exceptions::FormatError& e) {
				if (e.GetReason() != ::Assets::Exceptions::FormatError::Reason::UnsupportedVersion)
					throw;
			} CATCH(const Utility::Exceptions::IOException& e) {
				if (e.GetReason() != Utility::Exceptions::IOException::Reason::FileNotFound)
					throw;
			} CATCH_END

			if (asset) {
				auto wrapper = std::make_shared<std::unique_ptr<CompiledShaderByteCode>>(std::move(asset));
				std::function<std::unique_ptr<CompiledShaderByteCode>()> constructorCallback(
					[wrapper]() -> std::unique_ptr<CompiledShaderByteCode> { return std::move(*wrapper.get()); });
				auto result = std::make_shared<::Assets::DeferredConstruction>(
					nullptr, existingDepVal, std::move(constructorCallback));
				result->GetVariants().Add(ConstHash64<'Shad','erSt','age'>::Value, [stage](){ return stage; });
				return result;
			}
		}

		auto init0 = initializer.AsString();
		auto pendingCompile = marker->InvokeCompile();
		std::function<std::unique_ptr<CompiledShaderByteCode>()> constructorCallback(
			[pendingCompile, init0, stage]() -> std::unique_ptr<CompiledShaderByteCode> {
				auto state = pendingCompile->GetAssetState();
				if (state == ::Assets::AssetState::Pending)
					Throw(::Assets::Exceptions::PendingAsset(init0.c_str(), "Pending compilation operation"));
				if (state == ::Assets::AssetState::Invalid)
					Throw(::Assets::Exceptions::InvalidAsset(init0.c_str(), "Failure during compilation operation"));
				assert(state == ::Assets::AssetState::Ready);
                auto artifacts = pendingCompile->GetArtifacts();
                assert(artifacts.size() == 1);
				return ::Assets::Internal::ConstructFromIntermediateAssetLocator<CompiledShaderByteCode>(
					*artifacts[0].second, stage, MakeStringSection(init0));
			});
        #if defined(TEMP_HACK)
            assert(0);      // todo - need to set the dependency validation to something reasonable --
        #endif
		auto result = std::make_shared<::Assets::DeferredConstruction>(
			pendingCompile, nullptr, std::move(constructorCallback));
		result->GetVariants().Add(ConstHash64<'Shad','erSt','age'>::Value, [stage](){ return stage; });
		return result;
    }

	std::shared_ptr<::Assets::DeferredConstruction> CompiledShaderByteCode::BeginDeferredConstruction(
		const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount)
	{
		if (initializerCount == 1 || initializerCount == 2) {
			return BeginDeferredConstruction(initializers[0], (initializerCount>=2)?initializers[1]:StringSection<::Assets::ResChar>());
		} else if (initializerCount == 3 || initializerCount == 4) {
			return BeginDeferredConstruction(
				initializers[0], initializers[1], 
				initializers[2], (initializerCount>= 4)?initializers[3]:StringSection<::Assets::ResChar>());
		}
		return nullptr;
	}

    CompiledShaderByteCode::~CompiledShaderByteCode()
    {
    }

	void CompiledShaderByteCode::Resolve() const
	{
		if (!_shader && _deferredConstructor) {
			auto state = _deferredConstructor->GetAssetState();
			if (state == ::Assets::AssetState::Pending)
				Throw(Assets::Exceptions::PendingAsset(Initializer(), "Marker is still pending while resolving shader code"));

			if (state == ::Assets::AssetState::Ready) {
				auto* mutableThis = const_cast<CompiledShaderByteCode*>(this);
				auto constructor = std::move(mutableThis->_deferredConstructor);
				assert(!mutableThis->_deferredConstructor);
				*mutableThis = std::move(*constructor->PerformConstructor<CompiledShaderByteCode>());
				if (_shader && _shader->empty()) 
					_shader.reset();
			}
		}

		if (!_shader) {
			_shader.reset();
			Throw(Assets::Exceptions::InvalidAsset(Initializer(), "CompiledShaderByteCode invalid"));
		}
	}

    ::Assets::AssetState CompiledShaderByteCode::TryResolve() const
    {
        if (_shader)
            return ::Assets::AssetState::Ready;

        if (_deferredConstructor) {
			auto state = _deferredConstructor->GetAssetState();
			if (state != ::Assets::AssetState::Ready)
				return state;

			auto* mutableThis = const_cast<CompiledShaderByteCode*>(this);
			auto constructor = std::move(mutableThis->_deferredConstructor);
			assert(!mutableThis->_deferredConstructor);
			*mutableThis = std::move(*constructor->PerformConstructor<CompiledShaderByteCode>());
		
			if (_shader && !_shader->empty())
				return ::Assets::AssetState::Ready;

			_shader.reset();
		}

        return ::Assets::AssetState::Invalid;
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
        auto state = TryResolve();
        if (state != ::Assets::AssetState::Ready) return state;

        assert(_shader && !_shader->empty());
        byteCode = PtrAdd(AsPointer(_shader->begin()), sizeof(ShaderService::ShaderHeader));
        size = _shader->size() - sizeof(ShaderService::ShaderHeader);
        return ::Assets::AssetState::Ready;
    }

    std::shared_ptr<std::vector<uint8>> CompiledShaderByteCode::GetErrors() const
    {
        return std::shared_ptr<std::vector<uint8>>();
    }

    ::Assets::AssetState CompiledShaderByteCode::StallWhilePending() const
    {
        if (_shader)
            return ::Assets::AssetState::Ready;

        if (_deferredConstructor)
            return _deferredConstructor->StallWhilePending();

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
    , _deferredConstructor(std::move(moveFrom._deferredConstructor))
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
		_deferredConstructor = std::move(moveFrom._deferredConstructor);
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
        const ILowLevelCompiler* compiler) -> ResId
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
        if (compiler)
            compiler->AdaptShaderModel(shaderId._shaderModel, dimof(shaderId._shaderModel), shaderId._shaderModel);

        return std::move(shaderId);
    }

    auto ShaderService::CompileFromFile(
        StringSection<::Assets::ResChar> resId, 
        StringSection<::Assets::ResChar> definesTable) const -> std::shared_ptr<::Assets::PendingCompileMarker>
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
        StringSection<::Assets::ResChar> definesTable) const -> std::shared_ptr<::Assets::PendingCompileMarker>
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

    ShaderService::IShaderSource::~IShaderSource() {}
    ShaderService::ILowLevelCompiler::~ILowLevelCompiler() {}
}

