// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DrawableMaterial.h"
#include "Drawables.h"
#include "CompiledShaderPatchCollection.h"
#include "../Assets/ShaderPatchCollection.h"
#include "../../ShaderParser/ShaderPatcher.h"
#include "../../ShaderParser/NodeGraphProvider.h"
#include "../../Assets/DepVal.h"
#include "../../Utility/MemoryUtils.h"
#include <memory>

namespace RenderCore { namespace Techniques
{

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	static std::vector<std::pair<uint64_t, std::shared_ptr<CompiledShaderPatchCollection>>> s_compiledShaderPatchCollections;
	static std::shared_ptr<CompiledShaderPatchCollection> s_defaultCompiledShaderPatchCollection = std::make_shared<CompiledShaderPatchCollection>();

	const std::shared_ptr<CompiledShaderPatchCollection>& ShaderPatchCollectionRegistry::GetCompiledShaderPatchCollection(uint64_t hash) const
	{
		auto i = LowerBound(s_compiledShaderPatchCollections, hash);
		if (i != s_compiledShaderPatchCollections.end() && i->first == hash)
			return s_defaultCompiledShaderPatchCollection;
		return i->second;
	}

	void ShaderPatchCollectionRegistry::RegisterShaderPatchCollection(const RenderCore::Assets::ShaderPatchCollection& patchCollection) const
	{
		auto i = LowerBound(s_compiledShaderPatchCollections, patchCollection.GetHash());
		if (i != s_compiledShaderPatchCollections.end() && i->first == patchCollection.GetHash())
			return;		// already here

		s_compiledShaderPatchCollections.emplace(
			i, 
			std::make_pair(patchCollection.GetHash(), std::make_shared<CompiledShaderPatchCollection>(patchCollection)) );
	}

	ShaderPatchCollectionRegistry* ShaderPatchCollectionRegistry::s_instance = nullptr;

	ShaderPatchCollectionRegistry::ShaderPatchCollectionRegistry()
	{
		assert(!s_instance);
		s_instance = this;
	}

	ShaderPatchCollectionRegistry::~ShaderPatchCollectionRegistry() 
	{
		assert(s_instance == this);
		s_instance = nullptr;
		s_compiledShaderPatchCollections = decltype(s_compiledShaderPatchCollections){};
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	std::shared_ptr<DrawableMaterial> MakeDrawableMaterial(
		const RenderCore::Assets::MaterialScaffoldMaterial& mat,
		const RenderCore::Assets::ShaderPatchCollection& patchCollection)
	{
		auto result = std::make_shared<DrawableMaterial>();
		ShaderPatchCollectionRegistry::GetInstance().RegisterShaderPatchCollection(patchCollection);
		result->_patchCollection = ShaderPatchCollectionRegistry::GetInstance().GetCompiledShaderPatchCollection(patchCollection.GetHash());
		result->_material = mat;
		return result;
	}

}}

