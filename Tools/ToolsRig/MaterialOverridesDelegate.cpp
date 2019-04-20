// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialOverridesDelegate.h"
#include "../RenderCore/Techniques/BasicDelegates.h"
#include "../RenderCore/Assets/MaterialScaffold.h"
#include "../RenderCore/Assets/RawMaterial.h"
#include <unordered_map>

namespace ToolsRig
{
	class MaterialOverrideDelegate : public RenderCore::Techniques::MaterialDelegate_Basic
	{
	public:
		virtual RenderCore::UniformsStreamInterface GetInterface(const void* objectContext) const
		{
			return MaterialDelegate_Basic::GetInterface(&_material);
		}

		virtual uint64_t GetInterfaceHash(const void* objectContext) const
		{
			return MaterialDelegate_Basic::GetInterfaceHash(&_material);
		}

		virtual const ParameterBox* GetShaderSelectors(const void* objectContext) const
		{
			return MaterialDelegate_Basic::GetShaderSelectors(&_material);
		}

		virtual void ApplyUniforms(
			RenderCore::Techniques::ParsingContext& context,
			RenderCore::Metal::DeviceContext& devContext,
			const RenderCore::Metal::BoundUniforms& boundUniforms,
			unsigned streamIdx,
			const void* objectContext) const
		{
			MaterialDelegate_Basic::ApplyUniforms(
				context, devContext, boundUniforms,
				streamIdx, &_material);
		}

		MaterialOverrideDelegate(const RenderCore::Assets::MaterialScaffoldMaterial& material)
			: _material(material) {}
		~MaterialOverrideDelegate() {}
	private:
		RenderCore::Assets::MaterialScaffoldMaterial _material;
	};

	std::shared_ptr<RenderCore::Techniques::IMaterialDelegate>
		MakeMaterialOverrideDelegate(const RenderCore::Assets::MaterialScaffoldMaterial& material)
	{
		return std::make_shared<MaterialOverrideDelegate>(material);
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////.......

	class MaterialMergeDelegate : public RenderCore::Techniques::MaterialDelegate_Basic
	{
	public:
		virtual RenderCore::UniformsStreamInterface GetInterface(const void* objectContext) const override;
		virtual uint64_t GetInterfaceHash(const void* objectContext) const override;
		virtual const ParameterBox* GetShaderSelectors(const void* objectContext) const override;
		virtual void ApplyUniforms(
			RenderCore::Techniques::ParsingContext& context,
			RenderCore::Metal::DeviceContext& devContext,
			const RenderCore::Metal::BoundUniforms& boundUniforms,
			unsigned streamIdx,
			const void* objectContext) const override;

		MaterialMergeDelegate(
			const std::shared_ptr<RenderCore::Assets::RawMaterial>& mergeIn,
			const std::shared_ptr<RenderCore::Techniques::PredefinedCBLayout>& materialCBLayout);
		virtual ~MaterialMergeDelegate();
	private:
		std::shared_ptr<RenderCore::Assets::RawMaterial> _mergeIn;
		std::shared_ptr<RenderCore::Techniques::PredefinedCBLayout> _materialCBLayout;

		mutable std::unordered_map<const void*, RenderCore::Assets::MaterialScaffoldMaterial> _mergedResults;

		const RenderCore::Assets::MaterialScaffoldMaterial& GetMergedMaterial(const void* objectContext) const;
	};

	const RenderCore::Assets::MaterialScaffoldMaterial& MaterialMergeDelegate::GetMergedMaterial(const void* objectContext) const
	{
		auto i = _mergedResults.find(objectContext);
		if (i != _mergedResults.end())
			return i->second;

		RenderCore::Assets::MaterialScaffoldMaterial merged = *(const RenderCore::Assets::MaterialScaffoldMaterial*)objectContext;
		MergeInto(merged, *_mergeIn);
		auto i2 = _mergedResults.emplace(std::make_pair(objectContext, std::move(merged)));
		return i2.first->second;
	}

	RenderCore::UniformsStreamInterface MaterialMergeDelegate::GetInterface(const void* objectContext) const
	{
		return MaterialDelegate_Basic::GetInterface(&GetMergedMaterial(objectContext));
	}

	uint64_t MaterialMergeDelegate::GetInterfaceHash(const void* objectContext) const
	{
		// don't need to merge here, because the merged material interfaces can only differ if the input
		// materials differ
		return MaterialDelegate_Basic::GetInterfaceHash(objectContext);
	}

	const ParameterBox* MaterialMergeDelegate::GetShaderSelectors(const void* objectContext) const
	{
		return MaterialDelegate_Basic::GetShaderSelectors(&GetMergedMaterial(objectContext));
	}

	void MaterialMergeDelegate::ApplyUniforms(
		RenderCore::Techniques::ParsingContext& context,
		RenderCore::Metal::DeviceContext& devContext,
		const RenderCore::Metal::BoundUniforms& boundUniforms,
		unsigned streamIdx,
		const void* objectContext) const
	{
		MaterialDelegate_Basic::ApplyUniforms(
			context, devContext,
			boundUniforms, streamIdx, &GetMergedMaterial(objectContext),
			*_materialCBLayout);
	}

	MaterialMergeDelegate::MaterialMergeDelegate(
		const std::shared_ptr<RenderCore::Assets::RawMaterial>& mergeIn,
		const std::shared_ptr<RenderCore::Techniques::PredefinedCBLayout>& materialCBLayout)
	: _mergeIn(mergeIn)
	, _materialCBLayout(materialCBLayout)
	{
	}

	MaterialMergeDelegate::~MaterialMergeDelegate()
	{
	}

	std::shared_ptr<RenderCore::Techniques::IMaterialDelegate>
		MakeMaterialMergeDelegate(const std::shared_ptr<RenderCore::Assets::RawMaterial>& material,
		const std::shared_ptr<RenderCore::Techniques::PredefinedCBLayout>& materialCBLayout)
	{
		return std::make_shared<MaterialMergeDelegate>(material, materialCBLayout);
	}

}
