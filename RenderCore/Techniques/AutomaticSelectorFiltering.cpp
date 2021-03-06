// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AutomaticSelectorFiltering.h"
#include "../../Assets/InitializerPack.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/ICompileOperation.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/IntermediateCompilers.h"
#include "../../ConsoleRig/GlobalServices.h"		// for GetLibVersionDesc
#include "../../Utility/Streams/ConditionalPreprocessingTokenizer.h"
#include "../../Utility/Streams/OutputStreamFormatter.h"
#include "../../Utility/Streams/StreamTypes.h"
#include "../../Utility/Streams/SerializationUtils.h"
#include "../../Utility/Conversion.h"
#include "../../Utility/StringUtils.h"
#include <stdexcept>
#include <set>
#include <sstream>

namespace RenderCore { namespace Techniques
{
	static const auto ChunkType_Metrics = ConstHash64<'Metr', 'ics'>::Value;

	void SerializationOperator(
		Utility::OutputStreamFormatter& formatter,
		const ShaderSelectorFilteringRules& input)
	{
		auto e = formatter.BeginKeyedElement("TokenDictionary");
		for (const auto&t:input._tokenDictionary._tokenDefinitions)
			formatter.WriteSequencedValue(Concatenate(Utility::Internal::AsString(t._type), ":", t._value));
		formatter.EndElement(e);

		e = formatter.BeginKeyedElement("RelevanceTable");
		for (const auto&t:input._relevanceTable) {
			auto q = formatter.BeginKeyedElement(std::to_string(t.first));
			for (const auto&i:t.second)
				formatter.WriteSequencedValue(std::to_string(i));
			formatter.EndElement(q);
		}
		formatter.EndElement(e);
	}

	ShaderSelectorFilteringRules::ShaderSelectorFilteringRules(
		InputStreamFormatter<utf8>& formatter, 
		const ::Assets::DirectorySearchRules&,
		const ::Assets::DepValPtr& depVal)
	: _depVal(depVal)
	{
		StringSection<> keyedItem;
		while (formatter.TryKeyedItem(keyedItem)) {
			RequireBeginElement(formatter);
			if (XlEqString(keyedItem, "TokenDictionary")) {
				StringSection<> value;
				while (formatter.TryValue(value)) {
					auto colon = std::find(value.begin(), value.end(), ':');
					if (colon == value.end())
						Throw(Utility::FormatException("Missing colon in token", formatter.GetLocation()));

					Utility::Internal::TokenDictionary::Token token;
					token._type = Utility::Internal::AsTokenType({value.begin(), colon});
					token._value = std::string(colon+1, value.end());
					_tokenDictionary._tokenDefinitions.push_back(token);
				}
			} else if (XlEqString(keyedItem, "RelevanceTable")) {
				StringSection<> key;
				while (formatter.TryKeyedItem(key)) {
					Utility::Internal::ExpressionTokenList tokenList;
					RequireBeginElement(formatter);
					StringSection<> value;
					while (formatter.TryValue(value))
						tokenList.push_back(Conversion::Convert<unsigned>(value));
					RequireEndElement(formatter);
					_relevanceTable.insert(std::make_pair(
						Conversion::Convert<unsigned>(key),
						std::move(tokenList)));
				}
			} else {
				SkipElement(formatter);
			}
			RequireEndElement(formatter);
		}
	}

	ShaderSelectorFilteringRules::ShaderSelectorFilteringRules() {}
	ShaderSelectorFilteringRules::~ShaderSelectorFilteringRules() {}

	static ::Assets::Blob GenerateMetricsFile(const ShaderSelectorFilteringRules& rules)
	{
		std::stringstream str;
		str << "-------- Relevance Rules --------" << std::endl;
		for (const auto&r:rules._relevanceTable)
			str << "\t" << rules._tokenDictionary.AsString({r.first}) << " = " << rules._tokenDictionary.AsString(r.second) << std::endl;

		str << std::endl;
		str << "-------- Default Sets --------" << std::endl;
		for (const auto&r:rules._defaultSets) {
			if (!r.second.empty()) {
				str << "\t" << rules._tokenDictionary.AsString({r.first}) << " = " << rules._tokenDictionary.AsString(r.second) << std::endl;
			} else {
				str << "\t" << rules._tokenDictionary.AsString({r.first}) << " = <<no value>>" << std::endl;
			}
		}
		return ::Assets::AsBlob(str.str());
	}

	class IncludeHandler : public IPreprocessorIncludeHandler
	{
	public:
		virtual PreprocessorAnalysis GeneratePreprocessorAnalysis(
			StringSection<> requestString,
			StringSection<> fileIncludedFrom) override
		{
			std::string resolvedFile;
			if (!fileIncludedFrom.IsEmpty()) {
				char resolvedFileT[MaxPath];
				::Assets::DefaultDirectorySearchRules(fileIncludedFrom).ResolveFile(resolvedFileT, requestString);
				resolvedFile = resolvedFileT;
			} else {
				resolvedFile = requestString.AsString();
			}

			if (_processingFilesSet.find(resolvedFile) != _processingFilesSet.end())
				return {};		// recursive include -- trying to include a file while we're still processing it somewhere higher on the stack
			_processingFilesSet.insert(resolvedFile);

			::Assets::DependentFileState mainFileState;
			size_t size = 0;
			auto blk = ::Assets::TryLoadFileAsMemoryBlock(resolvedFile, &size, &mainFileState);
			if (!size)
				Throw(std::runtime_error("Missing or empty file when loading: " + resolvedFile + " (included from: " + fileIncludedFrom.AsString() + ")"));
			_depFileStates.insert(mainFileState);
			
			auto result = Utility::GeneratePreprocessorAnalysis(
				MakeStringSection((const char*)blk.get(), (const char*)PtrAdd(blk.get(), size)),
				resolvedFile,
				*this);

			_processingFilesSet.erase(resolvedFile);

			return result;
		}

		std::set<::Assets::DependentFileState> _depFileStates;

	private:
		std::set<std::string> _processingFilesSet;
	};

	class ShaderSelectorFilteringCompileOperation : public ::Assets::ICompileOperation
	{
	public:
		virtual std::vector<TargetDesc>			GetTargets() const override
		{
			return {
				TargetDesc { ShaderSelectorFilteringRules::CompileProcessType, "filtering-rules" }
			};
		}

		virtual std::vector<SerializedArtifact>	SerializeTarget(unsigned idx) override
		{
			assert(idx == 0);
			return _artifacts;
		}

		virtual std::vector<::Assets::DependentFileState> GetDependencies() const override
		{
			return _depFileStates;
		}

		ShaderSelectorFilteringCompileOperation(::Assets::InitializerPack& initializer)
		{
			auto fn = initializer.GetInitializer<std::string>(0);

			IncludeHandler handler;
			auto analysis = handler.GeneratePreprocessorAnalysis(fn, {});

			ShaderSelectorFilteringRules filteringRules;
			filteringRules._tokenDictionary = analysis._tokenDictionary;
			filteringRules._relevanceTable = analysis._relevanceTable;

			for (const auto&s:analysis._substitutionSideEffects._defaultSets)
				filteringRules._defaultSets.insert(std::make_pair(
					filteringRules._tokenDictionary.GetToken(Utility::Internal::TokenDictionary::TokenType::Variable, s.first),
					filteringRules._tokenDictionary.Translate(analysis._substitutionSideEffects._dictionary, s.second)));

			MemoryOutputStream<> memStream;
			OutputStreamFormatter fmttr(memStream);
			fmttr << filteringRules;

			{
				SerializedArtifact artifact;
				artifact._type = ShaderSelectorFilteringRules::CompileProcessType;
				artifact._name = "filtering-rules";
				artifact._version = 1;
				artifact._data = ::Assets::AsBlob(memStream.AsString());
				_artifacts.push_back(std::move(artifact));
			}

			{
				SerializedArtifact artifact;
				artifact._type = ChunkType_Metrics;
				artifact._name = "metrics";
				artifact._version = 1;
				artifact._data = GenerateMetricsFile(filteringRules);
				_artifacts.push_back(std::move(artifact));
			}

			_depFileStates.insert(_depFileStates.begin(), handler._depFileStates.begin(), handler._depFileStates.end());
		}

		std::vector<::Assets::DependentFileState> _depFileStates;
		std::vector<SerializedArtifact> _artifacts;
	};

	::Assets::IntermediateCompilers::CompilerRegistration RegisterShaderSelectorFilteringCompiler(
		::Assets::IntermediateCompilers& intermediateCompilers)
	{
		auto result = intermediateCompilers.RegisterCompiler(
			"shader-selector-filtering-compiler",
			ConsoleRig::GetLibVersionDesc(),
			nullptr,
			[](auto initializers) {
				return std::make_shared<ShaderSelectorFilteringCompileOperation>(initializers);
			});

		uint64_t outputAssetTypes[] = { ShaderSelectorFilteringRules::CompileProcessType };
		intermediateCompilers.AssociateRequest(
			result._registrationId,
			MakeIteratorRange(outputAssetTypes));
		return result;
	}
	
}}
