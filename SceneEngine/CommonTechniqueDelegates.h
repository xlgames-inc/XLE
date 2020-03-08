// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/AssetsCore.h"
#include <memory>

namespace RenderCore { namespace Techniques { 
	class TechniqueSetFile;
	class TechniqueSharedResources;
	class ITechniqueDelegate;
}}

namespace SceneEngine
{
	class CommonTechniqueDelegateBox
	{
	public:
		std::shared_ptr<RenderCore::Techniques::TechniqueSetFile> _techniqueSetFile;
		std::shared_ptr<RenderCore::Techniques::TechniqueSharedResources> _techniqueSharedResources;
		std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate> _forwardIllumDelegate_DisableDepthWrite;
		std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate> _depthOnlyDelegate;
		std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate> _deferredIllumDelegate;

		const ::Assets::DepValPtr& GetDependencyValidation() const;

		struct Desc {};
		CommonTechniqueDelegateBox(const Desc&);
	};
}


