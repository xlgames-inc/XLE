// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MinimalShaderSource.h"
#include "../Assets/IntermediateAssets.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/AssetUtils.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/StringFormat.h"

namespace RenderCore
{

	static ::Assets::DepValPtr AsDepValPtr(IteratorRange<const ::Assets::DependentFileState*> deps)
	{
		auto result = std::make_shared<::Assets::DependencyValidation>();
		for (const auto& i:deps)
			::Assets::RegisterFileDependency(result, MakeStringSection(i._filename));
		return result;
	}

#if 0
	class MinimalShaderSource::PendingMarker : public ::Assets::PendingCompileMarker
	{
	public:
		using Payload = std::shared_ptr<std::vector<uint8>>;
		Payload GetErrors() const;

		PendingMarker(
			Payload payload, 
			std::vector<::Assets::DependentFileState> deps, ShaderStage stage);
		PendingMarker(Payload errors);
		~PendingMarker();
		PendingMarker(const PendingMarker&) = delete;
		const PendingMarker& operator=(const PendingMarker&) = delete;
	private:
		ShaderStage _stage;
	};

    auto MinimalShaderSource::PendingMarker::Resolve(
        StringSection<::Assets::ResChar> initializer,
        const std::shared_ptr<::Assets::DependencyValidation>& depVal) const -> const Payload&
    {
        if (!_payload) {
            StringSection<char> errorsString;
            if (_errors)
                errorsString = StringSection<char>(
                    (const char*)AsPointer(_errors->cbegin()),
                    (const char*)AsPointer(_errors->cend()));

            Throw(::Assets::Exceptions::InvalidAsset(initializer, errorsString.AsString().c_str()));
        }

        if (depVal)
            for (const auto& i:_deps)
                ::Assets::RegisterFileDependency(depVal, MakeStringSection(i._filename));
        return _payload;
    }

    ::Assets::AssetState MinimalShaderSource::PendingMarker::TryResolve(
        Payload& result,
        const std::shared_ptr<::Assets::DependencyValidation>& depVal) const
    {
        if (!_payload) return ::Assets::AssetState::Invalid;
        result = _payload;
        if (depVal)
            for (const auto& i:_deps)
                ::Assets::RegisterFileDependency(depVal, MakeStringSection(i._filename));
        return ::Assets::AssetState::Ready;
    }

    auto MinimalShaderSource::PendingMarker::GetErrors() const -> Payload { return _errors; }

    ::Assets::AssetState MinimalShaderSource::PendingMarker::StallWhilePending() const 
    {
        return _payload ? ::Assets::AssetState::Ready : ::Assets::AssetState::Invalid;
    }

    ShaderStage::Enum MinimalShaderSource::PendingMarker::GetStage() const { return _stage; }

    MinimalShaderSource::PendingMarker::PendingMarker(
        Payload payload, 
        std::vector<::Assets::DependentFileState> deps, ShaderStage stage)
    : _payload(std::move(payload)), _deps(std::move(deps)), _stage(stage)
    {}
    MinimalShaderSource::PendingMarker::PendingMarker(Payload errors)
    : _errors(std::move(errors)), _stage(ShaderStage::Null) {}
    MinimalShaderSource::PendingMarker::~PendingMarker() {}
#endif

    auto MinimalShaderSource::Compile(
        const void* shaderInMemory, size_t size,
        const ShaderService::ResId& resId,
		StringSection<::Assets::ResChar> definesTable) const -> std::shared_ptr<::Assets::PendingCompileMarker>
    {
        using Payload = std::shared_ptr<std::vector<uint8>>;
        Payload payload, errors;
        std::vector<::Assets::DependentFileState> deps;

        bool success = _compiler->DoLowLevelCompile(
            payload, errors, deps,
            shaderInMemory, size, resId, definesTable);

		auto result = std::make_shared<::Assets::PendingCompileMarker>();
		result->GetLocator()._dependencyValidation = AsDepValPtr(MakeIteratorRange(deps));
		result->GetLocator()._errors = std::move(errors);
		result->GetLocator()._payload = std::move(payload);
		result->SetState(success ? ::Assets::AssetState::Ready : ::Assets::AssetState::Invalid);
		return std::move(result);
    }

    auto MinimalShaderSource::CompileFromFile(
		StringSection<::Assets::ResChar> resource, 
		StringSection<::Assets::ResChar> definesTable) const
        -> std::shared_ptr<::Assets::PendingCompileMarker>
    {
        auto resId = ShaderService::MakeResId(resource, *_compiler);

        size_t fileSize = 0;
        auto fileData = ::Assets::TryLoadFileAsMemoryBlock(resId._filename, &fileSize);
        return Compile(fileData.get(), fileSize, resId, definesTable);
    }
            
    auto MinimalShaderSource::CompileFromMemory(
		StringSection<char> shaderInMemory, StringSection<char> entryPoint, 
		StringSection<char> shaderModel, StringSection<::Assets::ResChar> definesTable) const
        -> std::shared_ptr<::Assets::PendingCompileMarker>
    {
        return Compile(
            shaderInMemory.begin(), shaderInMemory.size(),
            ShaderService::ResId(
                StringMeld<64>() << "ShaderInMemory_" << Hash64(shaderInMemory.begin(), shaderInMemory.end()), 
                entryPoint, shaderModel),
            definesTable);
    }

    MinimalShaderSource::MinimalShaderSource(std::shared_ptr<ShaderService::ILowLevelCompiler> compiler)
    : _compiler(compiler) {}
    MinimalShaderSource::~MinimalShaderSource() {}

}

