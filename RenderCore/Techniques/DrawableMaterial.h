// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/MaterialScaffold.h"
#include <memory>
#include <assert.h>

namespace RenderCore { namespace Assets { class ShaderPatchCollection; }}

namespace RenderCore { namespace Techniques
{
	class CompiledShaderPatchCollection;

	class DrawableMaterial
	{
	public:
		RenderCore::Assets::MaterialScaffoldMaterial _material;
		std::shared_ptr<CompiledShaderPatchCollection> _patchCollection;
	};

	class ShaderPatchCollectionRegistry
	{
	public:
		const std::shared_ptr<CompiledShaderPatchCollection>& GetCompiledShaderPatchCollection(uint64_t hash) const;
		void RegisterShaderPatchCollection(const RenderCore::Assets::ShaderPatchCollection& patchCollection) const;

		static ShaderPatchCollectionRegistry& GetInstance() { assert(s_instance); return *s_instance; }
		ShaderPatchCollectionRegistry();
		~ShaderPatchCollectionRegistry();
	private:
		static ShaderPatchCollectionRegistry* s_instance;
	};
}}

