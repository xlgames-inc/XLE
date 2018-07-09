// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "BasicDelegates.h"
#include "ResolvedTechniqueShaders.h"
#include "../../Assets/Assets.h"
#include "../../Utility/StringUtils.h"

namespace RenderCore { namespace Techniques
{

	RenderCore::UniformsStreamInterface MaterialDelegate_Basic::GetInterface(const void* objectContext) const
	{
		return {};
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
	}

    MaterialDelegate_Basic::MaterialDelegate_Basic()
	{
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

}}
