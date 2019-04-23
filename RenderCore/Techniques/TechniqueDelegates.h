// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DrawableDelegates.h"
#include "ResolvedTechniqueShaders.h"

namespace RenderCore { namespace Techniques
{
	class TechniqueSharedResources
	{
	public:
		ResolvedShaderVariationSet _mainVariationSet;
	};

	class TechniqueDelegate_Illum : public ITechniqueDelegate
	{
	public:
		virtual RenderCore::Metal::ShaderProgram* GetShader(
			ParsingContext& context,
			const ParameterBox* shaderSelectors[],
			RenderCore::Assets::CompiledShaderPatchCollection* patchCollection);

		TechniqueDelegate_Illum(const std::shared_ptr<TechniqueSharedResources>& sharedResources);
		~TechniqueDelegate_Illum();
	private:
		std::shared_ptr<TechniqueSharedResources> _sharedResources;

		::Assets::FuturePtr<Metal::ShaderProgram> MakeShaderVariation(
			const TechniqueEntry& techEntry,
			StringSection<> defines);

		::Assets::FuturePtr<TechniqueSetFile> _techniqueSetFuture;
		::Assets::DepValPtr _cfgFileDepVal;
		::Assets::AssetState _cfgFileState;
		TechniqueEntry _noPatches;
		TechniqueEntry _perPixel;
		TechniqueEntry _perPixelAndEarlyRejection;

		::Assets::AssetState PrimeTechniqueCfg();
	};

}}

