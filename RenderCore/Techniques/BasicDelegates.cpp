// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "BasicDelegates.h"
#include "ResolvedTechniqueShaders.h"
#include "ParsingContext.h"
#include "../Metal/InputLayout.h"
#include "../BufferView.h"
#include "../../Assets/Assets.h"
#include "../../Utility/StringUtils.h"

namespace RenderCore { namespace Techniques
{

	RenderCore::UniformsStreamInterface MaterialDelegate_Basic::GetInterface(const void* objectContext) const
	{
		RenderCore::UniformsStreamInterface result;
		result.BindConstantBuffer(0, {ObjectCB::BasicMaterialConstants});
		return result;
	}

    uint64_t MaterialDelegate_Basic::GetInterfaceHash(const void* objectContext) const
	{
		return 0;
	}

	const ParameterBox* MaterialDelegate_Basic::GetShaderSelectors(const void* objectContext) const
	{
		static ParameterBox dummy;
		return &dummy;
	}

    void MaterialDelegate_Basic::ApplyUniforms(
        ParsingContext& context,
        RenderCore::Metal::DeviceContext& devContext,
        const RenderCore::Metal::BoundUniforms& boundUniforms,
        unsigned streamIdx,
        const void* objectContext) const
	{
		ConstantBufferView cbvs[] = { _cbLayout.BuildCBDataAsPkt({}) };
		boundUniforms.Apply(
			devContext, streamIdx, 
			UniformsStream {
				MakeIteratorRange(cbvs)
			});
	}

    MaterialDelegate_Basic::MaterialDelegate_Basic()
	{
		const char cbLayout[] = R"--(
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

		_cbLayout = PredefinedCBLayout(cbLayout, true);
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
		auto techFuture = ::Assets::MakeAsset<ResolvedTechniqueShaders>(techniqueCfgFile);
		auto tech = techFuture->TryActualize();
		if (!tech) return nullptr;
		auto shaderFuture = tech->FindVariation(techniqueIndex, shaderSelectors);
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
