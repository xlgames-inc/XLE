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

	auto UniqueShaderVariationSet::FindVariation(
		const ShaderSelectors& baseSelectors,
		const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max],
		IShaderVariationFactory& factory) const -> const Variation&
	{
		auto inputHash = Hash(MakeIteratorRange(shaderSelectors, shaderSelectors + ShaderSelectors::Source::Max));
        
		uint64_t filteredHashValue;
        auto i = LowerBound(_globalToFiltered, inputHash);
        if (i!=_globalToFiltered.cend() && i->first == inputHash) {
            filteredHashValue = i->second;
        } else {
			filteredHashValue = baseSelectors.CalculateFilteredHash(inputHash, shaderSelectors);
			_globalToFiltered.insert(i, {inputHash, filteredHashValue});
		}

		filteredHashValue = HashCombine(filteredHashValue, factory._factoryGuid);

		auto i3 = std::lower_bound(
			_filteredToResolved.begin(), _filteredToResolved.end(), filteredHashValue,
			[](const Variation& v, uint64_t h) { return v._variationHash < h; });
        if (i3!=_filteredToResolved.cend() && i3->_variationHash == filteredHashValue) {
			if (i3->_shaderFuture->GetDependencyValidation() && i3->_shaderFuture->GetDependencyValidation()->GetValidationIndex()!=0)
				i3->_shaderFuture = MakeShaderVariation(baseSelectors, shaderSelectors, factory);
        } else {
			auto newVariation = MakeShaderVariation(baseSelectors, shaderSelectors, factory);
			i3 = _filteredToResolved.insert(i3, {filteredHashValue, newVariation});
		}

		return *i3;
	}

	static std::string MakeFilteredDefinesTable(
		IteratorRange<const ParameterBox**> selectors,
		const ParameterBox& baseTechniqueSelectors,
		const SelectorRelevanceMap& relevance)
	{
		// Selectors are considered relevant only if they appear in the 
		// baseTechniqueSelectors, or the condition in the relevance map succeeds

		std::vector<const ParameterBox*> selectorsWithBaseTechnique;
		selectorsWithBaseTechnique.reserve(1+selectors.size());
		selectorsWithBaseTechnique.push_back(&baseTechniqueSelectors);
		selectorsWithBaseTechnique.insert(selectorsWithBaseTechnique.begin(), selectors.begin(), selectors.end());

		ParameterBox filteredBox;

		for (const auto&b:selectors) {
			auto baseTechniqueIterator = baseTechniqueSelectors.begin();
			for (auto sourceIterator = b->begin(); sourceIterator != b->end(); ++sourceIterator) {

				// Look for the same selector in baseTechniqueSelectors
				while (baseTechniqueIterator != baseTechniqueSelectors.end() && baseTechniqueIterator->HashName() < sourceIterator->HashName()) 
					++baseTechniqueIterator;

				bool foundInBaseTechnique = baseTechniqueIterator != baseTechniqueSelectors.end() && baseTechniqueIterator->HashName() == sourceIterator->HashName();
				if (foundInBaseTechnique) {
					// If the value we're setting is the same as what's assigned in the base technique, then we can skip this selector
					// This just make it easier to shorten in the selector sets in some cases, because where setting a selector to
					// a specific value is equivalent to skipping that selector entirely, we should prefer to skip the selector
					bool identicalToBaseValue = false;
					if (baseTechniqueIterator->Type()._type != ImpliedTyping::TypeCat::Void
						&& sourceIterator->Type()._type != ImpliedTyping::TypeCat::Void) {
						identicalToBaseValue = baseTechniqueIterator->ValueAsString() == sourceIterator->ValueAsString();
					}

					if (!identicalToBaseValue) {
						// We found it in the base technique, so it's automatically relevant
						filteredBox.SetParameter(sourceIterator->Name(), sourceIterator->RawValue(), sourceIterator->Type());
						continue;		// early out since we've already determined we're relevant
					}
				}

				// see if we can pass the relevance check
				auto relevanceI = relevance.find(sourceIterator->Name().Cast<char>().AsString());
				if (relevanceI != relevance.end()) {
					bool relevant = EvaluatePreprocessorExpression(
						relevanceI->second,
						MakeIteratorRange(selectorsWithBaseTechnique));
					if (relevant)
						filteredBox.SetParameter(sourceIterator->Name(), sourceIterator->RawValue(), sourceIterator->Type());
				}
			}

		}

		return BuildFlatStringTable(filteredBox);
	}

	auto UniqueShaderVariationSet::FindVariation(
		IteratorRange<const ParameterBox**> selectors,
		const ParameterBox& baseTechniqueSelectors,
		const SelectorRelevanceMap& relevance,
		IShaderVariationFactory& factory) const -> const Variation&
	{
		auto inputHash = Hash(selectors);
		inputHash = HashCombine(baseTechniqueSelectors.GetParameterNamesHash(), inputHash);
		inputHash = HashCombine(baseTechniqueSelectors.GetHash(), inputHash);
		// todo -- we must include the relevance map in the hash

		std::string filteredDefinesTable;
		uint64_t filteredHashValue;

        auto i = LowerBound(_globalToFiltered, inputHash);
        if (i!=_globalToFiltered.cend() && i->first == inputHash) {
            filteredHashValue = i->second;
        } else {
			filteredDefinesTable = MakeFilteredDefinesTable(selectors, baseTechniqueSelectors, relevance);
			filteredHashValue = Hash64(filteredDefinesTable);
			_globalToFiltered.insert(i, {inputHash, filteredHashValue});
		}

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

	auto UniqueShaderVariationSet::MakeShaderVariation(
		const ShaderSelectors& baseSelectors,
		const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max],
		IShaderVariationFactory& factory) const -> ShaderFuture
	{
		std::vector<std::pair<const utf8*, std::string>> defines;
		baseSelectors.BuildStringTable(defines);
		for (unsigned c=0; c<ShaderSelectors::Source::Max; ++c) {
			OverrideStringTable(defines, *shaderSelectors[c]);
		}

		auto combinedStrings = FlattenStringTable(defines);
		return factory.MakeShaderVariation(MakeStringSection(combinedStrings));
	}

	UniqueShaderVariationSet::UniqueShaderVariationSet()  {}
	UniqueShaderVariationSet::~UniqueShaderVariationSet() {}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	::Assets::FuturePtr<Metal::ShaderProgram> TechniqueShaderVariationSet::FindVariation(
		int techniqueIndex,
		const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max]) const
	{
		const auto& techEntry = _technique->GetEntry(techniqueIndex);
		ShaderVariationFactory_Basic factory(techEntry);
        return _variationSet.FindVariation(techEntry._baseSelectors, shaderSelectors, factory)._shaderFuture;
	}

	TechniqueShaderVariationSet::TechniqueShaderVariationSet(const std::shared_ptr<Technique>& technique)
	: _technique(technique)
	{}

	TechniqueShaderVariationSet::~TechniqueShaderVariationSet(){}

	const ::Assets::DepValPtr& TechniqueShaderVariationSet::GetDependencyValidation()
	{
		return _technique->GetDependencyValidation();
	}

	void TechniqueShaderVariationSet::ConstructToFuture(
		::Assets::AssetFuture<TechniqueShaderVariationSet>& future,
		StringSection<::Assets::ResChar> modelScaffoldName)
	{
		auto scaffoldFuture = ::Assets::MakeAsset<Technique>(modelScaffoldName);

		future.SetPollingFunction(
			[scaffoldFuture](::Assets::AssetFuture<TechniqueShaderVariationSet>& thatFuture) -> bool {

			auto scaffoldActual = scaffoldFuture->TryActualize();

			if (!scaffoldActual) {
				auto state = scaffoldFuture->GetAssetState();
				if (state == ::Assets::AssetState::Invalid) {
					thatFuture.SetInvalidAsset(scaffoldFuture->GetDependencyValidation(), nullptr);
					return false;
				}
				return true;
			}

			auto newModel = std::make_shared<TechniqueShaderVariationSet>(scaffoldActual);
			thatFuture.SetAsset(std::move(newModel), {});
			return false;
		});
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	::Assets::FuturePtr<Metal::ShaderProgram> ShaderVariationFactory_Basic::MakeShaderVariation(StringSection<> defines) 
	{
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