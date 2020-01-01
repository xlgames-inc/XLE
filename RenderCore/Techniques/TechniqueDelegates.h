// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DrawableDelegates.h"
#include "ShaderVariationSet.h"

namespace RenderCore { namespace Techniques
{
	class TechniqueSharedResources
	{
	public:
		UniqueShaderVariationSet _mainVariationSet;
	};

	class TechniqueDelegate_Illum : public ITechniqueDelegate
	{
	public:
		virtual RenderCore::Metal::ShaderProgram* GetShader(
			ParsingContext& context,
			const ParameterBox* shaderSelectors[],
			const DrawableMaterial& material) override;

		TechniqueDelegate_Illum(const std::shared_ptr<TechniqueSharedResources>& sharedResources);
		TechniqueDelegate_Illum();
		~TechniqueDelegate_Illum();
	private:
		std::shared_ptr<TechniqueSharedResources> _sharedResources;

		::Assets::FuturePtr<TechniqueSetFile> _techniqueSetFuture;
		::Assets::DepValPtr _cfgFileDepVal;
		::Assets::AssetState _cfgFileState;
		TechniqueEntry _noPatches;
		TechniqueEntry _perPixel;
		TechniqueEntry _perPixelAndEarlyRejection;

		::Assets::AssetState PrimeTechniqueCfg();
	};


	/** <summary>Backwards compatibility for legacy style techniques</summary>
	This delegate allows for loading techniques from a legacy fixed function technique file.
	A default technique file is selected and the type of shader is picked via the technique
	index value. In this case, the material does now impact the technique selected.
	*/
	class TechniqueDelegate_Legacy : public ITechniqueDelegate
	{
	public:
		virtual RenderCore::Metal::ShaderProgram* GetShader(
			ParsingContext& context,
			const ParameterBox* shaderSelectors[],
			const DrawableMaterial& material,
			unsigned techniqueIndex) override;

		TechniqueDelegate_Legacy();
		~TechniqueDelegate_Legacy();

	private:
		::Assets::FuturePtr<Technique> _techniqueSetFuture;
		::Assets::DepValPtr _cfgFileDepVal;
		::Assets::AssetState _cfgFileState;

		std::shared_ptr<TechniqueShaderVariationSet> _variationSet;

		::Assets::AssetState PrimeTechniqueCfg();
	};

	std::shared_ptr<ITechniqueDelegate_New> CreateTechniqueDelegatePrototype(
		const std::shared_ptr<TechniqueSetFile>& techniqueSet,
		const std::shared_ptr<TechniqueSharedResources>& sharedResources);

}}

