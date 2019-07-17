// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CompiledShaderPatchCollection.h"
#include "../Assets/ShaderPatchCollection.h"
#include "../../ShaderParser/ShaderPatcher.h"
#include "../../ShaderParser/NodeGraphProvider.h"
#include "../../Assets/DepVal.h"
#include "../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Techniques
{
	static std::string Merge(const std::vector<std::string>& v)
	{
		size_t size=0;
		for (const auto&q:v) size += q.size();
		std::string result;
		result.reserve(size);
		for (const auto&q:v) result.insert(result.end(), q.begin(), q.end());
		return result;
	}

	static const auto s_perPixel = Hash64("PerPixel");
	static const auto s_earlyRejectionTest = Hash64("EarlyRejectionTest");

	CompiledShaderPatchCollection::CompiledShaderPatchCollection(const RenderCore::Assets::ShaderPatchCollection& src)
	{
		// With the given shader patch collection, build the source code and the 
		// patching functions associated
		
		_depVal = std::make_shared<::Assets::DependencyValidation>();
		_guid = src.GetHash();

		if (!src.GetPatches().empty()) {
			std::vector<ShaderSourceParser::InstantiationRequest_ArchiveName> finalInstRequests;
			finalInstRequests.reserve(src.GetPatches().size());
			for (const auto&i:src.GetPatches()) finalInstRequests.push_back(i.second);

			auto inst = InstantiateShader(MakeIteratorRange(finalInstRequests));
			_srcCode = Merge(inst._sourceFragments);

			_patches.reserve(inst._entryPoints.size());
			for (const auto&patch:inst._entryPoints) {
				if (patch._implementsName.empty()) continue;

				Patch p;
				p._implementsHash = Hash64(patch._implementsName);

				if (patch._implementsName != patch._name) {
					p._scaffoldInFunction = ShaderSourceParser::GenerateScaffoldFunction(
						patch._implementsSignature, patch._signature,
						patch._implementsName, patch._name, 
						ShaderSourceParser::ScaffoldFunctionFlags::ScaffoldeeUsesReturnSlot);
				}

				_patches.emplace_back(std::move(p));
			}

			for (const auto&d:inst._depVals)
				if (d)
					::Assets::RegisterAssetDependency(_depVal, d);
		}

		// Setup the precalculated values for the illum delegate
		_illumDelegate._type = IllumDelegateAttachment::IllumType::NoPerPixel;
		auto perPixel = std::find_if(
			_patches.begin(), _patches.end(),
			[](const RenderCore::Techniques::CompiledShaderPatchCollection::Patch& patch) { return patch._implementsHash == s_perPixel; });
		if (perPixel != _patches.end()) {
			auto earlyRejection = std::find_if(
				_patches.begin(), _patches.end(),
				[](const RenderCore::Techniques::CompiledShaderPatchCollection::Patch& patch) { return patch._implementsHash == s_earlyRejectionTest; });
			if (earlyRejection != _patches.end()) {
				_illumDelegate._type = IllumDelegateAttachment::IllumType::PerPixelAndEarlyRejection;
			} else {
				_illumDelegate._type = IllumDelegateAttachment::IllumType::PerPixel;
			}
		}
	}

	CompiledShaderPatchCollection::CompiledShaderPatchCollection() 
	{
		_depVal = std::make_shared<::Assets::DependencyValidation>();
	}

	CompiledShaderPatchCollection::~CompiledShaderPatchCollection() {}

}}
