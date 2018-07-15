// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ModelImmutableData.h"		// for SkeletonBinding
#include "../../Assets/AssetsCore.h"
#include "../../Math/Matrix.h"
#include <vector>
#include <memory>

namespace RenderCore { namespace Techniques { class Drawable; class DrawableGeo; }}
namespace RenderCore { class UniformsStreamInterface; }
namespace Utility { class VariantArray; }

namespace RenderCore { namespace Assets 
{
	class ModelScaffold;

	class SimpleModelRenderer
	{
	public:
		VariantArray BuildDrawables(uint64_t materialFilter = 0);

		SimpleModelRenderer(const std::shared_ptr<RenderCore::Assets::ModelScaffold>& modelScaffold);

		const ::Assets::DepValPtr& GetDependencyValidation();
		static void ConstructToFuture(
			::Assets::AssetFuture<SimpleModelRenderer>& future,
			StringSection<::Assets::ResChar> modelScaffoldName);
	private:
		std::shared_ptr<RenderCore::Assets::ModelScaffold> _modelScaffold;

		std::unique_ptr<Float4x4[]> _baseTransforms;
		unsigned _baseTransformCount;

		std::vector<std::shared_ptr<RenderCore::Techniques::DrawableGeo>> _geos;
		std::vector<std::shared_ptr<RenderCore::Techniques::DrawableGeo>> _boundSkinnedControllers;

		SkeletonBinding _skeletonBinding;

		std::shared_ptr<UniformsStreamInterface> _usi;
	};
}}