// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AutomaticSelectorFiltering.h"
#include "../Assets/InitializerPack.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/ICompileOperation.h"
#include "../Assets/AssetUtils.h"
#include "../Assets/IntermediateCompilers.h"
#include "../Assets/DepVal.h"
#include "../ConsoleRig/GlobalServices.h"		// for GetLibVersionDesc
#include "../Utility/Streams/ConditionalPreprocessingTokenizer.h"
#include "../Utility/Streams/OutputStreamFormatter.h"
#include "../Utility/Streams/StreamTypes.h"
#include "../Utility/Streams/SerializationUtils.h"
#include "../Utility/Conversion.h"
#include "../Utility/StringUtils.h"
#include "../Utility/FastParseValue.h"
#include <stdexcept>
#include <set>
#include <sstream>

namespace ShaderSourceParser
{
	static const auto ChunkType_Metrics = ConstHash64<'Metr', 'ics'>::Value;

	void SerializationOperator(
		Utility::OutputStreamFormatter& formatter,
		const SelectorFilteringRules& input)
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

	bool SelectorFilteringRules::IsRelevant(
		StringSection<> symbol, StringSection<> value,
		IteratorRange<const ParameterBox**> environment) const
	{
		bool passesRelevanceCheck = false;

		auto t = _tokenDictionary.TryGetToken(Utility::Internal::TokenDictionary::TokenType::Variable, symbol);
		if (t.has_value()) {
			auto i = _relevanceTable.find(t.value());
			if (i!=_relevanceTable.end()) {
				int relevanceCheck = _tokenDictionary.EvaluateExpression(i->second, environment);
				passesRelevanceCheck |= relevanceCheck != 0;
			}
		}

		if (!passesRelevanceCheck) {
			auto isDefinedT = _tokenDictionary.TryGetToken(Utility::Internal::TokenDictionary::TokenType::IsDefinedTest, symbol);
			if (isDefinedT.has_value()) {
				auto i = _relevanceTable.find(isDefinedT.value());
				if (i!=_relevanceTable.end()) {
					int relevanceCheck = _tokenDictionary.EvaluateExpression(i->second, environment);
					passesRelevanceCheck |= relevanceCheck != 0;
				}
			}
		}

		// Final check -- if we're setting to an integer value, and the shader has a defaulting
		// mechanism, then check if we're setting to the same value as the default
		if (passesRelevanceCheck && t.has_value() && !value.IsEmpty()) {
			int valueAsInt = 0;
			auto* end = FastParseValue(value, valueAsInt);
			if (end == value.end()) {
				auto i = _defaultSets.find(t.value());
				if (i!=_defaultSets.end()) {
					int defaultValue = _tokenDictionary.EvaluateExpression(i->second, environment);
					passesRelevanceCheck &= valueAsInt != defaultValue;
				}
			}
		}

		return passesRelevanceCheck;
	}

	void SelectorFilteringRules::MergeIn(const SelectorFilteringRules& source)
	{
		std::map<unsigned, Utility::Internal::ExpressionTokenList> translatedRelevance;
		for (const auto& e:source._relevanceTable) {
			translatedRelevance.insert(std::make_pair(
				_tokenDictionary.Translate(source._tokenDictionary, e.first),
				_tokenDictionary.Translate(source._tokenDictionary, e.second)));
		}

		_relevanceTable = Utility::Internal::MergeRelevanceTables(
			_relevanceTable, {},
			translatedRelevance, {});

		for (const auto& sideEffect:source._defaultSets) {
			auto key = _tokenDictionary.Translate(source._tokenDictionary, sideEffect.first);
			if (_defaultSets.find(key) != _defaultSets.end())
				continue;

			_defaultSets.insert(std::make_pair(
				key,
				_tokenDictionary.Translate(source._tokenDictionary, sideEffect.second)));
		}

		// Also need to merge in the dependency validation information
		if (_depVal) {
			auto newDepVal = std::make_shared<::Assets::DependencyValidation>();
			::Assets::RegisterAssetDependency(newDepVal, _depVal);
			::Assets::RegisterAssetDependency(newDepVal, source.GetDependencyValidation());
		} else {
			_depVal = source.GetDependencyValidation();
		}
		
		RecalculateHash();
	}

	void SelectorFilteringRules::RecalculateHash() 
	{
		// note -- if there are 2 different SelectorFilteringRules with exactly the same contents, except that the
		//		tokens are ordered differently in the _tokenDictionary, they will generate different hash values
		//		We could avoid that if we created a table of tokens -> hash values and used that to generate the value
		//		hash... But it seems pretty unlikely that that would be important
		_hash = _tokenDictionary.CalculateHash();
		for (const auto& r:_relevanceTable)
			_hash = HashCombine(Hash64(AsPointer(r.second.begin()), AsPointer(r.second.end()), r.first), _hash);
		for (const auto& r:_defaultSets)
			_hash = HashCombine(Hash64(AsPointer(r.second.begin()), AsPointer(r.second.end()), r.first), _hash);
	}

	SelectorFilteringRules::SelectorFilteringRules(
		InputStreamFormatter<utf8>& formatter, 
		const ::Assets::DirectorySearchRules&,
		const ::Assets::DepValPtr& depVal)
	: _depVal(depVal)
	{
		_tokenDictionary._tokenDefinitions.clear();	// empty starting/default tokens

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

		RecalculateHash();
	}

	SelectorFilteringRules::SelectorFilteringRules(
		const std::unordered_map<std::string, std::string>& relevanceStrings)
	{
		for (const auto&e:relevanceStrings) {
			auto key = Utility::Internal::AsExpressionTokenList(_tokenDictionary, e.first);
			if (key.size() != 1)
				Throw(std::runtime_error("Unexpected key in relevance strings: " + e.first));
			auto value = Utility::Internal::AsExpressionTokenList(
				_tokenDictionary, 
				e.second);
			_relevanceTable.insert(std::make_pair(key[0], std::move(value)));
		}
	}

	SelectorFilteringRules::SelectorFilteringRules() {}
	SelectorFilteringRules::~SelectorFilteringRules() {}

	SelectorFilteringRules GenerateSelectorFilteringRules(StringSection<> sourceCode)
	{
		auto analysis = Utility::GeneratePreprocessorAnalysis(sourceCode, {}, nullptr);
		SelectorFilteringRules filteringRules;

		filteringRules._tokenDictionary = analysis._tokenDictionary;
		filteringRules._relevanceTable = analysis._relevanceTable;

		for (const auto&s:analysis._substitutionSideEffects._defaultSets)
			filteringRules._defaultSets.insert(std::make_pair(
				filteringRules._tokenDictionary.GetToken(Utility::Internal::TokenDictionary::TokenType::Variable, s.first),
				filteringRules._tokenDictionary.Translate(analysis._substitutionSideEffects._dictionary, s.second)));

		return filteringRules;
	}

	std::ostream& SerializationOperator(std::ostream& str, const SelectorFilteringRules& rules)
	{
		str << "-------- Relevance Rules --------" << std::endl;
		for (const auto&r:rules._relevanceTable)
			str << "\t" << rules._tokenDictionary.AsString({r.first}) << " : " << rules._tokenDictionary.AsString(r.second) << std::endl;

		str << std::endl;
		str << "-------- Default Sets --------" << std::endl;
		for (const auto&r:rules._defaultSets) {
			if (!r.second.empty()) {
				str << "\t" << rules._tokenDictionary.AsString({r.first}) << " : " << rules._tokenDictionary.AsString(r.second) << std::endl;
			} else {
				str << "\t" << rules._tokenDictionary.AsString({r.first}) << " : <<no value>>" << std::endl;
			}
		}
		return str;
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
			if (!size) {
				if (!fileIncludedFrom.IsEmpty())
					Throw(std::runtime_error("Missing or empty file when loading: " + resolvedFile + " (included from: " + fileIncludedFrom.AsString() + ")"));
				Throw(std::runtime_error("Missing or empty file when loading: " + resolvedFile));
			}
			assert(!mainFileState._filename.empty());
			_depFileStates.insert(mainFileState);
			
			auto result = Utility::GeneratePreprocessorAnalysis(
				MakeStringSection((const char*)blk.get(), (const char*)PtrAdd(blk.get(), size)),
				resolvedFile,
				this);

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
				TargetDesc { SelectorFilteringRules::CompileProcessType, "filtering-rules" }
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

			SelectorFilteringRules filteringRules;
			filteringRules._tokenDictionary = analysis._tokenDictionary;
			filteringRules._relevanceTable = analysis._relevanceTable;

			for (auto& s:filteringRules._relevanceTable)
				filteringRules._tokenDictionary.Simplify(s.second);

			for (const auto&s:analysis._substitutionSideEffects._defaultSets) {
				auto trans = filteringRules._tokenDictionary.Translate(analysis._substitutionSideEffects._dictionary, s.second);
				filteringRules._tokenDictionary.Simplify(trans);
				filteringRules._defaultSets.insert(std::make_pair(
					filteringRules._tokenDictionary.GetToken(Utility::Internal::TokenDictionary::TokenType::Variable, s.first),
					trans));
			}

			MemoryOutputStream<> memStream;
			OutputStreamFormatter fmttr(memStream);
			fmttr << filteringRules;

			{
				SerializedArtifact artifact;
				artifact._type = SelectorFilteringRules::CompileProcessType;
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
				std::stringstream str;
				str << filteringRules;
				artifact._data = ::Assets::AsBlob(str.str());
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

		uint64_t outputAssetTypes[] = { SelectorFilteringRules::CompileProcessType };
		intermediateCompilers.AssociateRequest(
			result._registrationId,
			MakeIteratorRange(outputAssetTypes));
		return result;
	}
	
}
