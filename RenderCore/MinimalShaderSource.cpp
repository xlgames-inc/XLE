// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MinimalShaderSource.h"
#include "../Assets/IArtifact.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/ICompileOperation.h"
#include "../Assets/AssetUtils.h"
#include "../OSServices/RawFS.h"
#include "../Utility/StringFormat.h"
#include "../Utility/IteratorUtils.h"

namespace RenderCore
{
	static const auto ChunkType_Log = ConstHash64<'Log'>::Value;
	static const auto ChunkType_CompiledShaderByteCode = ConstHash64<'Shdr', 'Byte', 'Code'>::Value;

	class ShaderCompileOperation : public ::Assets::ICompileOperation
	{
	public:
		virtual std::vector<TargetDesc> GetTargets() const override
		{
			return {
				TargetDesc { ChunkType_CompiledShaderByteCode, "main" }
			};
		}
		
		virtual std::vector<SerializedArtifact> SerializeTarget(unsigned idx) override
		{
			return {
				::Assets::ICompileOperation::SerializedArtifact {
					ChunkType_CompiledShaderByteCode, 0,
					"main",
					_payload
				},
				::Assets::ICompileOperation::SerializedArtifact {
					ChunkType_Log, 0,
					"log",
					_errors
				}
			};
		}

		virtual std::vector<::Assets::DependentFileState> GetDependencies() const override
		{
			return _deps;
		}

		ShaderCompileOperation(
			ILowLevelCompiler& compiler,
			IteratorRange<const void*> pShaderInMemory,
			const ILowLevelCompiler::ResId& resId,
			StringSection<::Assets::ResChar> pDefinesTable);
		~ShaderCompileOperation();

		std::vector<::Assets::DependentFileState> _deps;
		::Assets::Blob _payload, _errors;		
	};

	ShaderCompileOperation::ShaderCompileOperation(
		ILowLevelCompiler& compiler,
		IteratorRange<const void*> pShaderInMemory,
		const ILowLevelCompiler::ResId& resId,
		StringSection<::Assets::ResChar> pDefinesTable)
	{
		std::vector<::Assets::DependentFileState> deps;
		::Assets::Blob payload, errors;
		bool success = false;
		TRY
		{
			success = compiler.DoLowLevelCompile(
				payload, errors, deps,
				pShaderInMemory.begin(), pShaderInMemory.size(), resId, 
				pDefinesTable);
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

		if (!success) {
			Throw(::Assets::Exceptions::ConstructionError(
				::Assets::Exceptions::ConstructionError::Reason::FormatNotUnderstood, 
				::Assets::AsDepVal(MakeIteratorRange(deps)),
				errors));
		}

		_deps = std::move(deps);
		_payload = std::move(payload);
		_errors = std::move(errors);
	}

	ShaderCompileOperation::~ShaderCompileOperation()
	{}

	auto MinimalShaderSource::Compile(
		IteratorRange<const void*> pShaderInMemory,
		const ILowLevelCompiler::ResId& resId,
		StringSection<::Assets::ResChar> pDefinesTable) const -> std::shared_ptr<::Assets::ArtifactCollectionFuture>
	{
		auto result = std::make_shared<::Assets::ArtifactCollectionFuture>();
		if (!(_flags & Flags::CompileInBackground)) {

			ShaderCompileOperation compileOp(*_compiler, pShaderInMemory, resId, pDefinesTable);
			auto depVal = AsDepVal(MakeIteratorRange(compileOp._deps));
			auto artifacts = compileOp.SerializeTarget(0);
			result->SetArtifactCollection(
				std::make_shared<::Assets::BlobArtifactCollection>(
					MakeIteratorRange(artifacts),
					depVal));

		} else {
			std::weak_ptr<ILowLevelCompiler> compiler = _compiler;
			std::string shaderInMemory{(const char*)pShaderInMemory.begin(), (const char*)pShaderInMemory.end()};
			std::string definesTable = pDefinesTable.AsString();

			std::function<void(::Assets::ArtifactCollectionFuture&)> operation =
				[compiler, shaderInMemory, resId, definesTable] (::Assets::ArtifactCollectionFuture& future) {

				auto c = compiler.lock();
				if (!c)
					Throw(std::runtime_error("Low level shader compiler has been destroyed before compile operation completed"));

				ShaderCompileOperation compileOp(*c, MakeIteratorRange(AsPointer(shaderInMemory.begin()), AsPointer(shaderInMemory.end())).Cast<const void*>(), resId, MakeStringSection(definesTable));
				auto depVal = AsDepVal(MakeIteratorRange(compileOp._deps));
				auto artifacts = compileOp.SerializeTarget(0);
				future.SetArtifactCollection(
					std::make_shared<::Assets::BlobArtifactCollection>(
						MakeIteratorRange(artifacts),
						depVal));
			};

			::Assets::QueueCompileOperation(result, std::move(operation));
		}
		return result;
	}

	auto MinimalShaderSource::CompileFromFile(
		StringSection<::Assets::ResChar> resource, 
		StringSection<::Assets::ResChar> definesTable) const
		-> std::shared_ptr<::Assets::ArtifactCollectionFuture>
	{
		auto resId = ShaderService::MakeResId(resource, _compiler.get());

		size_t fileSize = 0;
		auto fileData = ::Assets::TryLoadFileAsMemoryBlock(resId._filename, &fileSize);
		return Compile({fileData.get(), fileData.get() + fileSize}, resId, definesTable);
	}
			
	auto MinimalShaderSource::CompileFromMemory(
		StringSection<char> shaderInMemory, StringSection<char> entryPoint, 
		StringSection<char> shaderModel, StringSection<::Assets::ResChar> definesTable) const
		-> std::shared_ptr<::Assets::ArtifactCollectionFuture>
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

