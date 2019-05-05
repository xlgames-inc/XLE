// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CompiledShaderPatchCollection.h"
#include "TechniqueUtils.h"
#include "../Assets/ShaderPatchCollection.h"
#include "../Assets/PredefinedDescriptorSetLayout.h"
#include "../../ShaderParser/ShaderPatcher.h"
#include "../../ShaderParser/NodeGraphProvider.h"
#include "../../ShaderParser/ShaderAnalysis.h"
#include "../../Assets/DepVal.h"
#include "../../Assets/Assets.h"
#include "../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Techniques
{
	CompiledShaderPatchCollection::CompiledShaderPatchCollection(const RenderCore::Assets::ShaderPatchCollection& src)
	: _src(src)
	{
		_depVal = std::make_shared<::Assets::DependencyValidation>();
		_guid = src.GetHash();

		// With the given shader patch collection, build the source code and the 
		// patching functions associated
		
		if (!src.GetPatches().empty()) {
			std::vector<ShaderSourceParser::InstantiationRequest> finalInstRequests;
			finalInstRequests.reserve(src.GetPatches().size());
			for (const auto&i:src.GetPatches()) finalInstRequests.push_back(i.second);

			ShaderSourceParser::GenerateFunctionOptions generateOptions;
			auto inst = ShaderSourceParser::InstantiateShader(MakeIteratorRange(finalInstRequests), generateOptions, GetDefaultShaderLanguage());
			BuildFromInstantiatedShader(inst);
		}

		if (!_interface._descriptorSet && !src.GetDescriptorSet().empty()) {
			auto descriptorSetFuture = ::Assets::MakeAsset<RenderCore::Assets::PredefinedDescriptorSetLayout>(src.GetDescriptorSet());
			descriptorSetFuture->StallWhilePending();
			_interface._descriptorSet = descriptorSetFuture->Actualize();

			::Assets::RegisterAssetDependency(_depVal, _interface._descriptorSet->GetDependencyValidation());
		}
	}

	CompiledShaderPatchCollection::CompiledShaderPatchCollection(
		const ShaderSourceParser::InstantiatedShader& inst)
	{
		_depVal = std::make_shared<::Assets::DependencyValidation>();
		_guid = 0;
		BuildFromInstantiatedShader(inst);
	}

	void CompiledShaderPatchCollection::BuildFromInstantiatedShader(const ShaderSourceParser::InstantiatedShader& inst)
	{
			// Note -- we can build the patches interface here, because we assume that this will not
			//		even change with selectors

		_interface._patches.reserve(inst._entryPoints.size());
		for (const auto&patch:inst._entryPoints) {

			Interface::Patch p;
			if (!patch._implementsName.empty()) {
				p._implementsHash = Hash64(patch._implementsName);

				if (patch._implementsName != patch._name) {
					p._scaffoldInFunction = ShaderSourceParser::GenerateScaffoldFunction(
						patch._implementsSignature, patch._signature,
						patch._implementsName, patch._name, 
						ShaderSourceParser::ScaffoldFunctionFlags::ScaffoldeeUsesReturnSlot);
				}
			}

			p._signature = std::make_shared<GraphLanguage::NodeGraphSignature>(patch._signature);

			_interface._patches.emplace_back(std::move(p));
		}

		_interface._descriptorSet = inst._descriptorSet;

		for (const auto&d:inst._depVals)
			if (d)
				::Assets::RegisterAssetDependency(_depVal, d);
		for (const auto&d:inst._depFileStates)
			if (std::find(_dependencies.begin(), _dependencies.end(), d) == _dependencies.end())
				_dependencies.push_back(d);

		_interface._selectorRelevance = inst._selectorRelevance;
		if (!inst._rawShaderFileIncludes.empty()) {
			auto relevanceDepVal = ShaderSourceParser::Utility::MergeRelevanceFromShaderFiles(_interface._selectorRelevance, inst._rawShaderFileIncludes);
			if (relevanceDepVal)
				::Assets::RegisterAssetDependency(_depVal, relevanceDepVal);
		}

		size_t size = 0;
		for (const auto&i:inst._sourceFragments)
			size += i.size();
		_savedInstantiation.reserve(size);
		for (const auto&i:inst._sourceFragments)
			_savedInstantiation.insert(_savedInstantiation.end(), i.begin(), i.end());
	}

	CompiledShaderPatchCollection::CompiledShaderPatchCollection() 
	{
		_depVal = std::make_shared<::Assets::DependencyValidation>();
	}

	CompiledShaderPatchCollection::~CompiledShaderPatchCollection() {}

	static std::string Merge(const std::vector<std::string>& v)
	{
		size_t size=0;
		for (const auto&q:v) size += q.size();
		std::string result;
		result.reserve(size);
		for (const auto&q:v) result.insert(result.end(), q.begin(), q.end());
		return result;
	}

	std::string CompiledShaderPatchCollection::InstantiateShader(const ParameterBox& selectors) const
	{
		if (_src.GetPatches().empty()) {
			// If we'ved used  the constructor that takes a ShaderSourceParser::InstantiatedShader,
			// we can't re-instantiate here. So our only choice is to just return the saved
			// instantiation here. However, this means the selectors won't take effect, somewhat awkwardly
			return _savedInstantiation;
		}

		std::vector<ShaderSourceParser::InstantiationRequest> finalInstRequests;
		finalInstRequests.reserve(_src.GetPatches().size());
		for (const auto&i:_src.GetPatches()) finalInstRequests.push_back(i.second);

		ShaderSourceParser::GenerateFunctionOptions generateOptions;
		if (selectors.GetCount() != 0) {
			generateOptions._filterWithSelectors = true;
			generateOptions._selectors = selectors;
		}

		auto inst = ShaderSourceParser::InstantiateShader(MakeIteratorRange(finalInstRequests), generateOptions, GetDefaultShaderLanguage());
		return Merge(inst._sourceFragments);
	}

}}
