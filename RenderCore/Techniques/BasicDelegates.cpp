// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "BasicDelegates.h"
#include "ResolvedTechniqueShaders.h"
#include "ParsingContext.h"
#include "TechniqueMaterial.h"
#include "DeferredShaderResource.h"
#include "../Assets/PredefinedCBLayout.h"
#include "../Metal/InputLayout.h"
#include "../BufferView.h"
#include "../../Assets/Assets.h"
#include "../../Utility/StringUtils.h"

namespace RenderCore { namespace Techniques
{
	const char* g_techName = "xleres/Techniques/Illum.tech";

	RenderCore::UniformsStreamInterface MaterialDelegate_Basic::GetInterface(const void* objectContext) const
	{
		ScaffoldMaterial& mat = *(ScaffoldMaterial*)objectContext;
		RenderCore::UniformsStreamInterface result;
		result.BindConstantBuffer(0, {ObjectCB::BasicMaterialConstants});
		unsigned c=0;
		for (const auto& i:mat._bindings)
			result.BindShaderResource(c++, i.HashName());
		return result;
	}

    uint64_t MaterialDelegate_Basic::GetInterfaceHash(const void* objectContext) const
	{
		ScaffoldMaterial& mat = *(ScaffoldMaterial*)objectContext;
		return HashCombine(
			mat._bindings.GetParameterNamesHash(),
			Hash64(g_techName));
	}

	const ParameterBox* MaterialDelegate_Basic::GetShaderSelectors(const void* objectContext) const
	{
		ScaffoldMaterial& mat = *(ScaffoldMaterial*)objectContext;
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
		// ScaffoldMaterial& mat = *(ScaffoldMaterial*)objectContext;
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
		ScaffoldMaterial& mat = *(ScaffoldMaterial*)objectContext;
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
		
		ConstantBufferView cbvs[] = { cbLayout.BuildCBDataAsPkt(mat._constants) };
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

///////////////////////////////////////////////////////////////////////////////////////////////////

	RenderCore::Metal::ShaderProgram* TechniqueDelegate_Basic::GetShader(
		ParsingContext& context,
		StringSection<::Assets::ResChar> techniqueCfgFile,
		const ParameterBox* shaderSelectors[],		// ShaderSelectors::Source::Max
		unsigned techniqueIndex)
	{
		auto techFuture = ::Assets::MakeAsset<TechniqueShaderVariationSet>(techniqueCfgFile);
		auto tech = techFuture->TryActualize();
		if (!tech) return nullptr;
		const auto& shaderFuture = tech->FindVariation(techniqueIndex, shaderSelectors);
		if (!shaderFuture) return nullptr;
		return shaderFuture->TryActualize().get();
	}

	TechniqueDelegate_Basic::TechniqueDelegate_Basic()
	{
	}

	TechniqueDelegate_Basic::~TechniqueDelegate_Basic()
	{
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    ConstantBufferView GlobalCBDelegate::WriteBuffer(ParsingContext& context, const void* objectContext)
	{
		return context.GetGlobalCB(_cbIndex);
	}

    IteratorRange<const ConstantBufferElementDesc*> GlobalCBDelegate::GetLayout() const
	{
		return {};
	}

}}
