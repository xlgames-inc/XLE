// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderVariationSet.h"
#include "ParsingContext.h"
#include "../Types.h"
#include "../FrameBufferDesc.h"
#include "../UniformsStream.h"
#include "../BufferView.h"
#include "../Metal/InputLayout.h"
#include "../Metal/Shader.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/PipelineLayout.h"
#include "../../ShaderParser/AutomaticSelectorFiltering.h"
#include "../../ShaderParser/ShaderAnalysis.h"
#include "../../Assets/AssetsCore.h"
#include "../../Assets/Assets.h"
#include "../../Assets/AssetFutureContinuation.h"
#include "../../Utility/Streams/PreprocessorInterpreter.h"

namespace RenderCore { namespace Techniques
{
	static uint64_t Hash(IteratorRange<const ParameterBox**> shaderSelectors)
	{
		uint64_t inputHash = 0;
		const bool simpleHash = false;
		if (constant_expression<simpleHash>::result()) {
			for (unsigned c = 0; c < shaderSelectors.size(); ++c) {
				inputHash ^= shaderSelectors[c]->GetParameterNamesHash();
				inputHash ^= shaderSelectors[c]->GetHash() << (c * 6);    // we have to be careful of cases where the values in one box is very similar to the values in another
			}
		} else {
			inputHash = HashCombine(shaderSelectors[0]->GetHash(), shaderSelectors[0]->GetParameterNamesHash());
			for (unsigned c = 1; c < shaderSelectors.size(); ++c) {
				inputHash = HashCombine(shaderSelectors[c]->GetParameterNamesHash(), inputHash);
				inputHash = HashCombine(shaderSelectors[c]->GetHash(), inputHash);
			}
		}
		return inputHash;
	}

	static std::string MakeFilteredDefinesTable(
		IteratorRange<const ParameterBox**> selectors,
		const ShaderSourceParser::ManualSelectorFiltering& techniqueFiltering,
		const ShaderSourceParser::SelectorFilteringRules& automaticFiltering)
	{
		return BuildFlatStringTable(ShaderSourceParser::FilterSelectors(
			selectors, techniqueFiltering, automaticFiltering));
	}

	auto UniqueShaderVariationSet::FilterSelectors(
		IteratorRange<const ParameterBox**> selectors,
		const ShaderSourceParser::ManualSelectorFiltering& techniqueFiltering,
		const ShaderSourceParser::SelectorFilteringRules& automaticFiltering) -> FilteredSelectorSet
	{
		auto inputHash = Hash(selectors);
		inputHash = HashCombine(techniqueFiltering.GetHash(), inputHash);
		inputHash = HashCombine(automaticFiltering.GetHash(), inputHash);

		auto i = LowerBound(_globalToFiltered, inputHash);
		if (i!=_globalToFiltered.cend() && i->first == inputHash) {
			return i->second;
		} else {
			FilteredSelectorSet result;
			result._selectors = MakeFilteredDefinesTable(selectors, techniqueFiltering, automaticFiltering);
			result._hashValue = Hash64(result._selectors);
			_globalToFiltered.insert(i, {inputHash, result});
			return result;
		}
	}

	UniqueShaderVariationSet::UniqueShaderVariationSet()  {}
	UniqueShaderVariationSet::~UniqueShaderVariationSet() {}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	::Assets::FuturePtr<Metal::ShaderProgram> TechniqueShaderVariationSet::FindVariation(
		int techniqueIndex,
		const ParameterBox* shaderSelectors[SelectorStages::Max]) const
	{
		const auto& techEntry = _technique->GetEntry(techniqueIndex);
		assert(0); // need to pass ICompiledPipelineLayout down
		// ShaderVariationFactory_Basic factory(techEntry, nullptr);
		// return _variationSet.FindVariation(techEntry._selectorFiltering, shaderSelectors, factory)._shaderFuture;
		return nullptr;
	}

	TechniqueShaderVariationSet::TechniqueShaderVariationSet(const std::shared_ptr<Technique>& technique)
	: _technique(technique)
	{}

	TechniqueShaderVariationSet::~TechniqueShaderVariationSet(){}

	const ::Assets::DepValPtr& TechniqueShaderVariationSet::GetDependencyValidation() const
	{
		return _technique->GetDependencyValidation();
	}

	void TechniqueShaderVariationSet::ConstructToFuture(
		::Assets::AssetFuture<TechniqueShaderVariationSet>& future,
		StringSection<::Assets::ResChar> modelScaffoldName)
	{
		auto scaffoldFuture = ::Assets::MakeAsset<Technique>(modelScaffoldName);
		::Assets::WhenAll(scaffoldFuture).ThenConstructToFuture<TechniqueShaderVariationSet>(future);
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	::Assets::FuturePtr<Metal::ShaderProgram> ShaderVariationFactory_Basic::MakeShaderVariation(StringSection<> defines) 
	{
		assert(!_entry->_vertexShaderName.empty());
		assert(!_entry->_pixelShaderName.empty());
		if (_entry->_geometryShaderName.empty()) {
			return ::Assets::MakeAsset<Metal::ShaderProgram>(_pipelineLayout, _entry->_vertexShaderName, _entry->_pixelShaderName, defines);
		} else {
			return ::Assets::MakeAsset<Metal::ShaderProgram>(_pipelineLayout, _entry->_vertexShaderName, _entry->_geometryShaderName, _entry->_pixelShaderName, defines);
		}
	}

	ShaderVariationFactory_Basic::ShaderVariationFactory_Basic(const TechniqueEntry& entry, const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout)
	: _entry(&entry)
	, _pipelineLayout(pipelineLayout)
	{
		_factoryGuid = entry._shaderNamesHash;
	}

	ShaderVariationFactory_Basic::~ShaderVariationFactory_Basic() {}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	IShaderVariationFactory::~IShaderVariationFactory() {}
}}