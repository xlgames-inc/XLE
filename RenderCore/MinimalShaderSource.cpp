// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MinimalShaderSource.h"
#include "../Assets/IArtifact.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/ICompileOperation.h"
#include "../Assets/AssetUtils.h"
#include "../Assets/InitializerPack.h"
#include "../ConsoleRig/GlobalServices.h"		// for ConsoleRig::GetLibVersionDesc()
#include "../Utility/IteratorUtils.h"

namespace RenderCore
{
	static const auto ChunkType_Log = ConstHash64<'Log'>::Value;
	static const auto ChunkType_CompiledShaderByteCode = ConstHash64<'Shdr', 'Byte', 'Code'>::Value;

	class MinimalShaderSource::Pimpl
	{
	public:
		std::shared_ptr<ILowLevelCompiler> _compiler;
		std::shared_ptr<ISourceCodePreprocessor> _preprocessor;
	};

	auto MinimalShaderSource::Compile(
		StringSection<> shaderInMemory,
		const ILowLevelCompiler::ResId& resId,
		StringSection<::Assets::ResChar> definesTable) const -> ShaderByteCodeBlob
	{
		ShaderByteCodeBlob result;
		bool success = false;
		TRY
		{
			if (_pimpl->_preprocessor) {
				auto preprocessedOutput = _pimpl->_preprocessor->RunPreprocessor(
					shaderInMemory, definesTable,
					::Assets::DefaultDirectorySearchRules(resId._filename));
				if (preprocessedOutput._processedSource.empty())
					Throw(std::runtime_error("Preprocessed output is empty"));

				result._deps = std::move(preprocessedOutput._dependencies);

				success = _pimpl->_compiler->DoLowLevelCompile(
					result._payload, result._errors, result._deps,
					preprocessedOutput._processedSource.data(), preprocessedOutput._processedSource.size(), resId,
					definesTable,
					MakeIteratorRange(preprocessedOutput._lineMarkers));

			} else {
				success = _pimpl->_compiler->DoLowLevelCompile(
					result._payload, result._errors, result._deps,
					shaderInMemory.begin(), shaderInMemory.size(), resId, 
					definesTable);
			}
		}
			// embue any exceptions with the dependency validation
		CATCH(const ::Assets::Exceptions::ConstructionError& e)
		{
			result._errors = ::Assets::AsBlob(e.what());
		}
		CATCH(const std::exception& e)
		{
			result._errors = ::Assets::AsBlob(e.what());
		}
		CATCH_END

		(void)success;

		return result;
	}

	auto MinimalShaderSource::CompileFromFile(
		const ILowLevelCompiler::ResId& resId, 
		StringSection<::Assets::ResChar> definesTable) const
		-> ShaderByteCodeBlob
	{
		size_t fileSize = 0;
		auto fileData = ::Assets::TryLoadFileAsMemoryBlock(resId._filename, &fileSize);
		return Compile({(const char*)fileData.get(), (const char*)fileData.get() + fileSize}, resId, definesTable);
	}
			
	auto MinimalShaderSource::CompileFromMemory(
		StringSection<char> shaderInMemory, StringSection<char> entryPoint, 
		StringSection<char> shaderModel, StringSection<::Assets::ResChar> definesTable) const
		-> ShaderByteCodeBlob
	{
		return Compile(
			shaderInMemory,
			ILowLevelCompiler::ResId("", entryPoint, shaderModel),		// use an empty string for the filename here, beacuse otherwhile it tends to confuse the DX11 compiler (when generating error messages, it will treat the string as a filename from the current directory)
			definesTable);
	}

	MinimalShaderSource::MinimalShaderSource(
		const std::shared_ptr<ILowLevelCompiler>& compiler, 
		const std::shared_ptr<ISourceCodePreprocessor>& preprocessor)
	{
		_pimpl = std::make_unique<Pimpl>();
		_pimpl->_compiler = compiler;
		_pimpl->_preprocessor = preprocessor;
	}
	MinimalShaderSource::~MinimalShaderSource() {}

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
			std::vector<SerializedArtifact> result;
			if (_byteCode._payload)
				result.push_back({
					ChunkType_CompiledShaderByteCode, 0, "main",
					_byteCode._payload});
			if (_byteCode._errors)
				result.push_back({
					ChunkType_Log, 0, "log",
					_byteCode._errors});
			return result;
		}

		virtual std::vector<::Assets::DependentFileState> GetDependencies() const override
		{
			return _byteCode._deps;
		}

		ShaderCompileOperation(
			ShaderService::IShaderSource& shaderSource,
			const ILowLevelCompiler::ResId& resId,
			StringSection<> definesTable)
		: _byteCode { shaderSource.CompileFromFile(resId, definesTable) }
		{
		}
		
		~ShaderCompileOperation()
		{
		}

		ShaderService::IShaderSource::ShaderByteCodeBlob _byteCode;
	};

	::Assets::IntermediateCompilers::CompilerRegistration RegisterShaderCompiler(
		const std::shared_ptr<ShaderService::IShaderSource>& shaderSource,
		::Assets::IntermediateCompilers& intermediateCompilers)
	{
		auto result = intermediateCompilers.RegisterCompiler(
			"shader-compiler",
			ConsoleRig::GetLibVersionDesc(),
			nullptr,
			[shaderSource](const ::Assets::InitializerPack& initializers) {
				return std::make_shared<ShaderCompileOperation>(
					*shaderSource,
					ShaderService::MakeResId(initializers.GetInitializer<std::string>(0)),
					initializers.GetInitializer<std::string>(1)
				);
			});

		uint64_t outputAssetTypes[] = { CompiledShaderByteCode::CompileProcessType };
		intermediateCompilers.AssociateRequest(
			result._registrationId,
			MakeIteratorRange(outputAssetTypes));
		return result;
	}

}

