// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MinimalShaderSource.h"
#include "../Assets/AssetUtils.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/StringFormat.h"

namespace RenderCore
{

    auto MinimalShaderSource::PendingMarker::Resolve(
        const char initializer[],
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
        std::vector<::Assets::DependentFileState> deps, ShaderStage::Enum stage)
    : _payload(std::move(payload)), _deps(std::move(deps)), _stage(stage)
    {}
    MinimalShaderSource::PendingMarker::PendingMarker(Payload errors)
    : _errors(std::move(errors)), _stage(ShaderStage::Null) {}
    MinimalShaderSource::PendingMarker::~PendingMarker() {}

    auto MinimalShaderSource::Compile(
        const void* shaderInMemory, size_t size,
        const ShaderService::ResId& resId,
        const ::Assets::ResChar definesTable[]) const -> std::shared_ptr<IPendingMarker>
    {
        using Payload = PendingMarker::Payload;
        Payload payload, errors;
        std::vector<::Assets::DependentFileState> deps;

        bool success = _compiler->DoLowLevelCompile(
            payload, errors, deps,
            shaderInMemory, size, resId, definesTable);

        if (!success)
            return std::make_shared<PendingMarker>(errors);
        return std::make_shared<PendingMarker>(
            std::move(payload), std::move(deps), resId.AsShaderStage());
    }

    auto MinimalShaderSource::CompileFromFile(
        const ::Assets::ResChar resource[], 
        const ::Assets::ResChar definesTable[]) const
        -> std::shared_ptr<IPendingMarker>
    {
        auto resId = ShaderService::MakeResId(resource, *_compiler);

        size_t fileSize = 0;
        auto fileData = LoadFileAsMemoryBlock(resId._filename, &fileSize);
        return Compile(fileData.get(), fileSize, resId, definesTable);
    }
            
    auto MinimalShaderSource::CompileFromMemory(
        const char shaderInMemory[], const char entryPoint[], 
        const char shaderModel[], const ::Assets::ResChar definesTable[]) const
        -> std::shared_ptr<IPendingMarker>
    {
        auto shaderLen = XlStringLen(shaderInMemory);
        return Compile(
            shaderInMemory, shaderLen,
            ShaderService::ResId(
                StringMeld<64>() << "ShaderInMemory_" << Hash64(shaderInMemory, &shaderInMemory[shaderLen]), 
                entryPoint, shaderModel),
            definesTable);
    }

    MinimalShaderSource::MinimalShaderSource(std::shared_ptr<ShaderService::ILowLevelCompiler> compiler)
    : _compiler(compiler) {}
    MinimalShaderSource::~MinimalShaderSource() {}

}

