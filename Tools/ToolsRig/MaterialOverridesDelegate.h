// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Techniques/DrawableDelegates.h"

namespace RenderCore { namespace Assets { class MaterialScaffoldMaterial; class RawMaterial; class PredefinedCBLayout; } }

namespace ToolsRig
{
	std::shared_ptr<RenderCore::Techniques::IMaterialDelegate>
		MakeMaterialOverrideDelegate(const RenderCore::Assets::MaterialScaffoldMaterial& material);

	/// <summary>Material delegate that injects some property overrides</summary>
	/// This object is intended for tools and previewing purposes. It will mix together
	/// material settings from the material basic delegate with overrides from a
	/// fixed material object
	std::shared_ptr<RenderCore::Techniques::IMaterialDelegate>
		MakeMaterialMergeDelegate(
			const std::shared_ptr<RenderCore::Assets::RawMaterial>& material,
			const std::shared_ptr<RenderCore::Assets::PredefinedCBLayout>& materialCBLayout);

}
