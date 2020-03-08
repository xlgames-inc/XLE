// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CommonTechniqueDelegates.h"
#include "../RenderCore/Techniques/TechniqueDelegates.h"
#include "../Assets/AssetTraits.h"
#include "../xleres/FileList.h"

namespace SceneEngine
{
	const ::Assets::DepValPtr& CommonTechniqueDelegateBox::GetDependencyValidation() const { return _techniqueSetFile->GetDependencyValidation(); }

	CommonTechniqueDelegateBox::CommonTechniqueDelegateBox(const Desc&)
	{
		_techniqueSetFile = ::Assets::AutoConstructAsset<RenderCore::Techniques::TechniqueSetFile>(ILLUM_TECH);
		_techniqueSharedResources = std::make_shared<RenderCore::Techniques::TechniqueSharedResources>();
		_forwardIllumDelegate_DisableDepthWrite = RenderCore::Techniques::CreateTechniqueDelegate_Forward(_techniqueSetFile, _techniqueSharedResources, RenderCore::Techniques::TechniqueDelegateForwardFlags::DisableDepthWrite);
		_depthOnlyDelegate = RenderCore::Techniques::CreateTechniqueDelegate_DepthOnly(_techniqueSetFile, _techniqueSharedResources);
		_deferredIllumDelegate = RenderCore::Techniques::CreateTechniqueDelegate_Deferred(_techniqueSetFile, _techniqueSharedResources);
	}
}
