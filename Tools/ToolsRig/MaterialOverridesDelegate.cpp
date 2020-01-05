// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialOverridesDelegate.h"
#include "../RenderCore/Techniques/BasicDelegates.h"
#include "../RenderCore/Techniques/CommonBindings.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"
#include "../RenderCore/Techniques/DeferredShaderResource.h"
#include "../RenderCore/Assets/MaterialScaffold.h"
#include "../RenderCore/Assets/RawMaterial.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/BufferView.h"
#include "../../Assets/Assets.h"
#include <unordered_map>

namespace RenderCore { namespace Techniques
{
	// Legacy material delegate implementation. Only used here atm

	class MaterialDelegate_Basic : public IMaterialDelegate
    {
    public:
        virtual RenderCore::UniformsStreamInterface GetInterface(const void* objectContext) const;
        virtual uint64_t GetInterfaceHash(const void* objectContext) const;
		virtual const ParameterBox* GetShaderSelectors(const void* objectContext) const;
        virtual void ApplyUniforms(
            ParsingContext& context,
            RenderCore::Metal::DeviceContext& devContext,
            const RenderCore::Metal::BoundUniforms& boundUniforms,
            unsigned streamIdx,
            const void* objectContext) const;

        MaterialDelegate_Basic();
		~MaterialDelegate_Basic();

	protected:
		void MaterialDelegate_Basic::ApplyUniforms(
			ParsingContext& context,
			RenderCore::Metal::DeviceContext& devContext,
			const RenderCore::Metal::BoundUniforms& boundUniforms,
			unsigned streamIdx,
			const void* objectContext,
			const RenderCore::Assets::PredefinedCBLayout& cbLayout) const;
    };

	const char* g_techName = "xleres/Techniques/Illum.tech";

	RenderCore::UniformsStreamInterface MaterialDelegate_Basic::GetInterface(const void* objectContext) const
	{
		auto& mat = *(RenderCore::Assets::MaterialScaffoldMaterial*)objectContext;
		RenderCore::UniformsStreamInterface result;
		result.BindConstantBuffer(0, {ObjectCB::BasicMaterialConstants});
		unsigned c=0;
		for (const auto& i:mat._bindings)
			result.BindShaderResource(c++, i.HashName());
		return result;
	}

    uint64_t MaterialDelegate_Basic::GetInterfaceHash(const void* objectContext) const
	{
		auto& mat = *(RenderCore::Assets::MaterialScaffoldMaterial*)objectContext;
		return HashCombine(
			mat._bindings.GetParameterNamesHash(),
			Hash64(g_techName));
	}

	const ParameterBox* MaterialDelegate_Basic::GetShaderSelectors(const void* objectContext) const
	{
		auto& mat = *(RenderCore::Assets::MaterialScaffoldMaterial*)objectContext;
		static ParameterBox dummy;
		dummy = ParameterBox();
		for (const auto& i:mat._bindings)
			dummy.SetParameter(MakeStringSection(std::basic_string<utf8>(u("RES_HAS_")) + i.Name().begin()), 1);
		for (const auto& i:mat._matParams)
			dummy.SetParameter(i.Name(), i.RawValue(), i.Type());
		return &dummy;
	}

	void MaterialDelegate_Basic::ApplyUniforms(
		ParsingContext& context,
		RenderCore::Metal::DeviceContext& devContext,
		const RenderCore::Metal::BoundUniforms& boundUniforms,
		unsigned streamIdx,
		const void* objectContext) const
	{
		// auto& mat = *(RenderCore::Assets::MaterialScaffoldMaterial*)objectContext;
		auto techniqueFuture = ::Assets::MakeAsset<Technique>(g_techName);
		const auto& cbLayout = techniqueFuture->Actualize()->TechniqueCBLayout();
		return ApplyUniforms(
			context, devContext, boundUniforms,
			streamIdx, objectContext, cbLayout);
	}

	void MaterialDelegate_Basic::ApplyUniforms(
		ParsingContext& context,
		RenderCore::Metal::DeviceContext& devContext,
		const RenderCore::Metal::BoundUniforms& boundUniforms,
		unsigned streamIdx,
		const void* objectContext,
		const RenderCore::Assets::PredefinedCBLayout& cbLayout) const
	{
		auto& mat = *(RenderCore::Assets::MaterialScaffoldMaterial*)objectContext;
		const RenderCore::Metal::ShaderResourceView* srvs[32];
		unsigned c=0;
		for (const auto&i:mat._bindings) {
			if (!i.RawValue().empty()) {
				auto future = ::Assets::MakeAsset<DeferredShaderResource>(
					MakeStringSection((char*)i.RawValue().begin(), (char*)i.RawValue().end()));
				srvs[c++] = &future->Actualize()->GetShaderResource();
			} else {
				srvs[c++] = nullptr;
			}
		}
		
		ConstantBufferView cbvs[] = { cbLayout.BuildCBDataAsPkt(mat._constants, RenderCore::Techniques::GetDefaultShaderLanguage()) };
		boundUniforms.Apply(
			devContext, streamIdx, 
			UniformsStream {
				MakeIteratorRange(cbvs),
				IteratorRange<const void*const*>{(const void*const*)srvs, (const void*const*)&srvs[c]}
			});
	}

    MaterialDelegate_Basic::MaterialDelegate_Basic()
	{
		/*const char cbLayout[] = R"--(
			float3  MaterialDiffuse = {1.f,1.f,1.f}c;
			float   Opacity = 1;
			float3  MaterialSpecular = {1.f,1.f,1.f}c;
			float   AlphaThreshold = .33f;
			float   RoughnessMin = 0.5f;
			float   RoughnessMax = 1.f;
			float   SpecularMin = 0.1f;
			float   SpecularMax = 1.f;
			float   MetalMin = 0.f;
			float   MetalMax = 1.f;)--";

		_cbLayout = PredefinedCBLayout(cbLayout, true);*/
	}

	MaterialDelegate_Basic::~MaterialDelegate_Basic() 
	{
	}
}}

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
			const std::shared_ptr<RenderCore::Assets::PredefinedCBLayout>& materialCBLayout);
		virtual ~MaterialMergeDelegate();
	private:
		std::shared_ptr<RenderCore::Assets::RawMaterial> _mergeIn;
		std::shared_ptr<RenderCore::Assets::PredefinedCBLayout> _materialCBLayout;

		mutable std::unordered_map<const void*, RenderCore::Assets::MaterialScaffoldMaterial> _mergedResults;

		const RenderCore::Assets::MaterialScaffoldMaterial& GetMergedMaterial(const void* objectContext) const;
	};

	const RenderCore::Assets::MaterialScaffoldMaterial& MaterialMergeDelegate::GetMergedMaterial(const void* objectContext) const
	{
		auto i = _mergedResults.find(objectContext);
		if (i != _mergedResults.end())
			return i->second;

		RenderCore::Assets::MaterialScaffoldMaterial merged = *(const RenderCore::Assets::MaterialScaffoldMaterial*)objectContext;
		RenderCore::Assets::ShaderPatchCollection patchCollection;
		MergeInto(merged, patchCollection, *_mergeIn);
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
		const std::shared_ptr<RenderCore::Assets::PredefinedCBLayout>& materialCBLayout)
	: _mergeIn(mergeIn)
	, _materialCBLayout(materialCBLayout)
	{
	}

	MaterialMergeDelegate::~MaterialMergeDelegate()
	{
	}

	std::shared_ptr<RenderCore::Techniques::IMaterialDelegate>
		MakeMaterialMergeDelegate(const std::shared_ptr<RenderCore::Assets::RawMaterial>& material,
		const std::shared_ptr<RenderCore::Assets::PredefinedCBLayout>& materialCBLayout)
	{
		return std::make_shared<MaterialMergeDelegate>(material, materialCBLayout);
	}

}
