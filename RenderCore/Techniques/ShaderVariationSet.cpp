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

	std::string MakeFilteredDefinesTable(
		IteratorRange<const ParameterBox**> selectors,
		const ShaderSelectorFiltering& techniqueFiltering,
		const SelectorRelevanceMap& relevance)
	{
		// Selectors are considered relevant only if they appear in the 
		// baseTechniqueSelectors, or the condition in the relevance map succeeds

		ParameterBox pBoxValue;

		std::vector<const ParameterBox*> selectorsWithBaseTechnique;
		selectorsWithBaseTechnique.reserve(2+selectors.size());
		selectorsWithBaseTechnique.push_back(&pBoxValue);
		selectorsWithBaseTechnique.push_back(&techniqueFiltering._setValues);
		selectorsWithBaseTechnique.insert(selectorsWithBaseTechnique.begin(), selectors.begin(), selectors.end());

		ParameterBox filteredBox = techniqueFiltering._setValues;

		for (const auto&b:selectors) {
			auto setIterator = techniqueFiltering._setValues.begin();
			for (auto sourceIterator = b->begin(); sourceIterator != b->end(); ++sourceIterator) {

				bool hasSetValue = false;
				bool passesRelevanceMap = false;
				bool passesTechniqueRelevanceMap = false;

				// Set values (note -- blacklist doesn't apply here)
				while (setIterator != techniqueFiltering._setValues.end() && setIterator->HashName() < sourceIterator->HashName()) 
					++setIterator;
				if (setIterator != techniqueFiltering._setValues.end() && setIterator->HashName() == sourceIterator->HashName()) {
					hasSetValue = true;
					passesTechniqueRelevanceMap = true;		// considered relevant, unless we explicitly fail in the condition just below
				}

				auto relevanceI = techniqueFiltering._relevanceMap.find(sourceIterator->Name().AsString());
				if (relevanceI != techniqueFiltering._relevanceMap.end()) {
					// Set a key called "value" to the new value we want to set
					pBoxValue.SetParameter("value", sourceIterator->RawValue(), sourceIterator->Type());
					passesTechniqueRelevanceMap = EvaluatePreprocessorExpression(
						relevanceI->second,
						MakeIteratorRange(selectorsWithBaseTechnique));
				}

				// see if we can pass the relevance check
				relevanceI = relevance.find(sourceIterator->Name().AsString());
				if (relevanceI != relevance.end()) {
					passesRelevanceMap = EvaluatePreprocessorExpression(
						relevanceI->second,
						MakeIteratorRange(AsPointer(selectorsWithBaseTechnique.begin()+1), AsPointer(selectorsWithBaseTechnique.end())));
				}

				if (passesRelevanceMap || passesTechniqueRelevanceMap) {
					filteredBox.SetParameter(sourceIterator->Name(), sourceIterator->RawValue(), sourceIterator->Type());
				} else {
					filteredBox.RemoveParameter(sourceIterator->Name());
				}
			}

		}

		return BuildFlatStringTable(filteredBox);
	}

	auto UniqueShaderVariationSet::FindVariation(
		IteratorRange<const ParameterBox**> selectors,
		const ShaderSelectorFiltering& techniqueFiltering,
		const SelectorRelevanceMap& relevance,
		IShaderVariationFactory& factory) const -> const Variation&
	{
		auto inputHash = Hash(selectors);
		inputHash = HashCombine(techniqueFiltering.GetHash(), inputHash);
		// todo -- we must include the relevance map in the hash

		std::string filteredDefinesTable;
		uint64_t filteredHashValue;

        auto i = LowerBound(_globalToFiltered, inputHash);
        if (i!=_globalToFiltered.cend() && i->first == inputHash) {
            filteredHashValue = i->second._filteredHashValue;
			filteredDefinesTable = i->second._filteredSelectors;
        } else {
			filteredDefinesTable = MakeFilteredDefinesTable(selectors, techniqueFiltering, relevance);
			filteredHashValue = Hash64(filteredDefinesTable);
			_globalToFiltered.insert(i, {inputHash, FilteredDefines{filteredHashValue, filteredDefinesTable}});
		}

		filteredHashValue = HashCombine(filteredHashValue, factory._factoryGuid);

		auto i3 = std::lower_bound(
			_filteredToResolved.begin(), _filteredToResolved.end(), filteredHashValue,
			[](const Variation& v, uint64_t h) { return v._variationHash < h; });
        if (i3!=_filteredToResolved.cend() && i3->_variationHash == filteredHashValue) {
			if (i3->_shaderFuture->GetDependencyValidation() && i3->_shaderFuture->GetDependencyValidation()->GetValidationIndex()!=0)
				i3->_shaderFuture = factory.MakeShaderVariation(MakeStringSection(filteredDefinesTable));
        } else {
			auto newVariation = factory.MakeShaderVariation(MakeStringSection(filteredDefinesTable));
			i3 = _filteredToResolved.insert(i3, {filteredHashValue, newVariation});
		}

		return *i3;
	}

	auto UniqueShaderVariationSet::FindVariation(
		const ShaderSelectorFiltering& techniqueFiltering,
		const ParameterBox* shaderSelectors[ShaderSelectorFiltering::Source::Max],
		IShaderVariationFactory& factory) const -> const Variation&
	{
		return FindVariation(
			MakeIteratorRange(shaderSelectors, shaderSelectors + ShaderSelectorFiltering::Source::Max),
			techniqueFiltering, 
			{},
			factory);
	}

	UniqueShaderVariationSet::UniqueShaderVariationSet()  {}
	UniqueShaderVariationSet::~UniqueShaderVariationSet() {}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	::Assets::FuturePtr<Metal::ShaderProgram> TechniqueShaderVariationSet::FindVariation(
		int techniqueIndex,
		const ParameterBox* shaderSelectors[ShaderSelectorFiltering::Source::Max]) const
	{
		const auto& techEntry = _technique->GetEntry(techniqueIndex);
		ShaderVariationFactory_Basic factory(techEntry);
        return _variationSet.FindVariation(techEntry._selectorFiltering, shaderSelectors, factory)._shaderFuture;
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
			return ::Assets::MakeAsset<Metal::ShaderProgram>(_entry->_vertexShaderName, _entry->_pixelShaderName, defines);
		} else {
			return ::Assets::MakeAsset<Metal::ShaderProgram>(_entry->_vertexShaderName, _entry->_geometryShaderName, _entry->_pixelShaderName, defines);
		}
	}

	ShaderVariationFactory_Basic::ShaderVariationFactory_Basic(const TechniqueEntry& entry) 
	: _entry(&entry) 
	{
		_factoryGuid = entry._shaderNamesHash;
	}

	ShaderVariationFactory_Basic::~ShaderVariationFactory_Basic() {}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	IShaderVariationFactory::~IShaderVariationFactory() {}
}}