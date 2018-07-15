// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MinimalShaderSource.h"
#include "../Assets/IAssetCompiler.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/AssetUtils.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/IteratorUtils.h"

namespace RenderCore
{

    auto MinimalShaderSource::Compile(
        const void* shaderInMemory, size_t size,
        const ShaderService::ResId& resId,
		StringSection<::Assets::ResChar> definesTable) const -> std::shared_ptr<::Assets::CompileFuture>
    {
        using Payload = ::Assets::Blob;
        Payload payload, errors;
        std::vector<::Assets::DependentFileState> deps;

        bool success = _compiler->DoLowLevelCompile(
            payload, errors, deps,
            shaderInMemory, size, resId, definesTable);

		auto result = std::make_shared<::Assets::CompileFuture>();
        auto depVal = AsDepVal(MakeIteratorRange(deps));
		result->AddArtifact("main", std::make_shared<Assets::BlobArtifact>(payload, std::move(depVal)));
		result->AddArtifact("log", std::make_shared<Assets::BlobArtifact>(errors, std::move(depVal)));
		result->SetState(success ? ::Assets::AssetState::Ready : ::Assets::AssetState::Invalid);
		return result;
    }

    auto MinimalShaderSource::CompileFromFile(
		StringSection<::Assets::ResChar> resource, 
		StringSection<::Assets::ResChar> definesTable) const
        -> std::shared_ptr<::Assets::CompileFuture>
    {
        auto resId = ShaderService::MakeResId(resource, _compiler.get());

        size_t fileSize = 0;
        auto fileData = ::Assets::TryLoadFileAsMemoryBlock(resId._filename, &fileSize);
        return Compile(fileData.get(), fileSize, resId, definesTable);
    }
            
    auto MinimalShaderSource::CompileFromMemory(
		StringSection<char> shaderInMemory, StringSection<char> entryPoint, 
		StringSection<char> shaderModel, StringSection<::Assets::ResChar> definesTable) const
        -> std::shared_ptr<::Assets::CompileFuture>
    {
        return Compile(
            shaderInMemory.begin(), shaderInMemory.size(),
            ShaderService::ResId(
                StringMeld<64>() << "ShaderInMemory_" << Hash64(shaderInMemory.begin(), shaderInMemory.end()), 
                entryPoint, shaderModel),
            definesTable);
    }

	void MinimalShaderSource::ClearCaches()
	{
	}

    MinimalShaderSource::MinimalShaderSource(std::shared_ptr<ShaderService::ILowLevelCompiler> compiler)
    : _compiler(compiler) {}
    MinimalShaderSource::~MinimalShaderSource() {}

}

