// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CompiledShaderPatchCollection.h"
#include "TechniqueUtils.h"
#include "../Assets/ShaderPatchCollection.h"
#include "../../ShaderParser/ShaderPatcher.h"
#include "../../ShaderParser/NodeGraphProvider.h"
#include "../../ShaderParser/ShaderAnalysis.h"
#include "../../Assets/DepVal.h"
#include "../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Techniques
{
	CompiledShaderPatchCollection::CompiledShaderPatchCollection(const RenderCore::Assets::ShaderPatchCollection& src)
	: _src(src)
	{
		// With the given shader patch collection, build the source code and the 
		// patching functions associated
		
		_depVal = std::make_shared<::Assets::DependencyValidation>();
		_guid = src.GetHash();

		ShaderSourceParser::GenerateFunctionOptions generateOptions;

		if (!src.GetPatches().empty()) {
			std::vector<ShaderSourceParser::InstantiationRequest> finalInstRequests;
			finalInstRequests.reserve(src.GetPatches().size());
			for (const auto&i:src.GetPatches()) finalInstRequests.push_back(i.second);

			auto inst = ShaderSourceParser::InstantiateShader(MakeIteratorRange(finalInstRequests), generateOptions, GetDefaultShaderLanguage());

			// Note -- we can build the patches interface here, because we assume that this will not
			//		even change with selectors

			_interface._patches.reserve(inst._entryPoints.size());
			for (const auto&patch:inst._entryPoints) {
				if (patch._implementsName.empty()) continue;

				Interface::Patch p;
				p._implementsHash = Hash64(patch._implementsName);

				if (patch._implementsName != patch._name) {
					p._scaffoldInFunction = ShaderSourceParser::GenerateScaffoldFunction(
						patch._implementsSignature, patch._signature,
						patch._implementsName, patch._name, 
						ShaderSourceParser::ScaffoldFunctionFlags::ScaffoldeeUsesReturnSlot);
				}

				_interface._patches.emplace_back(std::move(p));
			}

			_interface._descriptorSet = inst._descriptorSet;

			for (const auto&d:inst._depVals)
				if (d)
					::Assets::RegisterAssetDependency(_depVal, d);

			_interface._selectorRelevance = inst._selectorRelevance;
			if (!inst._rawShaderFileIncludes.empty()) {
				auto relevanceDepVal = ShaderSourceParser::Utility::MergeRelevanceFromShaderFiles(_interface._selectorRelevance, inst._rawShaderFileIncludes);
				if (relevanceDepVal)
					::Assets::RegisterAssetDependency(_depVal, relevanceDepVal);
			}
		}
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
		if (_src.GetPatches().empty())
			return {};

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
