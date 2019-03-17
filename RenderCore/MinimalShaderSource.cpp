// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MinimalShaderSource.h"
#include "../Assets/IArtifact.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/AssetUtils.h"
#include "../Assets/CompilationThread.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/IteratorUtils.h"

namespace RenderCore
{

    auto MinimalShaderSource::Compile(
        IteratorRange<const void*> pShaderInMemory,
        const ILowLevelCompiler::ResId& resId,
		StringSection<::Assets::ResChar> pDefinesTable) const -> std::shared_ptr<::Assets::ArtifactFuture>
    {
		auto result = std::make_shared<::Assets::ArtifactFuture>();
		if (!(_flags & Flags::CompileInBackground)) {
			using Payload = ::Assets::Blob;
			Payload payload, errors;
			std::vector<::Assets::DependentFileState> deps;

			bool success = _compiler->DoLowLevelCompile(
				payload, errors, deps,
				pShaderInMemory.begin(), pShaderInMemory.size(), resId, pDefinesTable);

			auto depVal = AsDepVal(MakeIteratorRange(deps));
			result->AddArtifact("main", std::make_shared<Assets::BlobArtifact>(payload, std::move(depVal)));
			result->AddArtifact("log", std::make_shared<Assets::BlobArtifact>(errors, std::move(depVal)));
			result->SetState(success ? ::Assets::AssetState::Ready : ::Assets::AssetState::Invalid);
		} else {
			std::weak_ptr<ILowLevelCompiler> compiler = _compiler;
			std::string shaderInMemory{(const char*)pShaderInMemory.begin(), (const char*)pShaderInMemory.end()};
			std::string definesTable = pDefinesTable.AsString();

			std::function<void(::Assets::ArtifactFuture&)> operation =
				[compiler, shaderInMemory, resId, definesTable] (::Assets::ArtifactFuture& future) {

				auto c = compiler.lock();
				if (!c)
					Throw(std::runtime_error("Low level shader compiler has been destroyed before compile operation completed"));

				std::vector<::Assets::DependentFileState> deps;
				::Assets::Blob errors, payload;
				bool success = false;

				TRY
				{
					success = c->DoLowLevelCompile(
						payload, errors, deps,
						shaderInMemory.data(), shaderInMemory.size(), resId,
						definesTable);
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

					// Create the artifact and add it to the compile marker
				auto depVal = ::Assets::AsDepVal(MakeIteratorRange(deps));
				auto newState = success ? ::Assets::AssetState::Ready : ::Assets::AssetState::Invalid;

				future.AddArtifact("main", std::make_shared<::Assets::BlobArtifact>(payload, depVal));
				future.AddArtifact("log", std::make_shared<::Assets::BlobArtifact>(errors, depVal));

					// give the ArtifactFuture object the same state
				assert(future.GetArtifacts().size() != 0);
				future.SetState(newState);
			};

			::Assets::QueueCompileOperation(result, std::move(operation));
		}
		return result;
    }

    auto MinimalShaderSource::CompileFromFile(
		StringSection<::Assets::ResChar> resource, 
		StringSection<::Assets::ResChar> definesTable) const
        -> std::shared_ptr<::Assets::ArtifactFuture>
    {
        auto resId = ShaderService::MakeResId(resource, _compiler.get());

        size_t fileSize = 0;
        auto fileData = ::Assets::TryLoadFileAsMemoryBlock(resId._filename, &fileSize);
		return Compile({fileData.get(), fileData.get() + fileSize}, resId, definesTable);
    }
            
    auto MinimalShaderSource::CompileFromMemory(
		StringSection<char> shaderInMemory, StringSection<char> entryPoint, 
		StringSection<char> shaderModel, StringSection<::Assets::ResChar> definesTable) const
        -> std::shared_ptr<::Assets::ArtifactFuture>
    {
        return Compile(
			{shaderInMemory.begin(), shaderInMemory.end()},
            ILowLevelCompiler::ResId("", entryPoint, shaderModel),		// use an empty string for the filename here, beacuse otherwhile it tends to confuse the DX11 compiler (when generating error messages, it will treat the string as a filename from the current directory)
            definesTable);
    }

	void MinimalShaderSource::ClearCaches()
	{
	}

    MinimalShaderSource::MinimalShaderSource(const std::shared_ptr<ILowLevelCompiler>& compiler, Flags::BitField flags)
    : _compiler(compiler), _flags(flags) {}
    MinimalShaderSource::~MinimalShaderSource() {}

}

