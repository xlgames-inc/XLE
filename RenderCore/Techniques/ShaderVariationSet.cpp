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

namespace RenderCore { namespace Techniques
{
	static uint64_t Hash64(const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max])
	{
		uint64_t inputHash = 0;
		const bool simpleHash = false;
		if (constant_expression<simpleHash>::result()) {
			for (unsigned c = 0; c < ShaderSelectors::Source::Max; ++c) {
				inputHash ^= shaderSelectors[c]->GetParameterNamesHash();
				inputHash ^= shaderSelectors[c]->GetHash() << (c * 6);    // we have to be careful of cases where the values in one box is very similar to the values in another
			}
		} else {
			inputHash = HashCombine(shaderSelectors[0]->GetHash(), shaderSelectors[0]->GetParameterNamesHash());
			for (unsigned c = 1; c < ShaderSelectors::Source::Max; ++c) {
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
		auto inputHash = Hash64(shaderSelectors);
        
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