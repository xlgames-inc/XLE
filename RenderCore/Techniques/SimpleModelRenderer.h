// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/ModelImmutableData.h"		// for SkeletonBinding
#include "../Metal/Forward.h"
#include "../../Math/Matrix.h"
#include "../../Assets/AssetsCore.h"
#include "../../Utility/StringUtils.h"
#include <vector>
#include <memory>

namespace RenderCore { namespace Assets 
{ 
	class ModelScaffold;
	class MaterialScaffold;
}}
namespace RenderCore { class IThreadContext; class IResource; class UniformsStreamInterface; }
namespace Utility { class VariantArray; }

namespace RenderCore { namespace Techniques 
{
	class Drawable; class DrawableGeo;
	class DrawablesPacket; 
	class ParsingContext; 
	class PipelineAcceleratorPool; 
	class PipelineAccelerator; 
	class DescriptorSetAccelerator;
	class DeformOperationInstantiation;
	class IDeformOperation;

	class SimpleModelRenderer
	{
	public:
		void BuildDrawables(
			IteratorRange<DrawablesPacket** const> pkts,
			const Float4x4& localToWorld = Identity<Float4x4>()) const;

		class IPreDrawDelegate
		{
		public:
			virtual bool OnDraw(
				Metal::DeviceContext&, ParsingContext&,
				const Drawable&,
				uint64_t materialGuid, unsigned drawCallIdx) = 0;
			virtual ~IPreDrawDelegate();
		};

		void BuildDrawables(
			IteratorRange<DrawablesPacket** const> pkts,
			const Float4x4& localToWorld,
			const std::shared_ptr<IPreDrawDelegate>& delegate) const;

		void GenerateDeformBuffer(IThreadContext& context);
		unsigned DeformOperationCount() const;
		IDeformOperation& DeformOperation(unsigned idx);
		const ::Assets::DepValPtr& GetDependencyValidation();

		const std::shared_ptr<RenderCore::Assets::ModelScaffold>& GetModelScaffold() const { return _modelScaffold; }
		const std::shared_ptr<RenderCore::Assets::MaterialScaffold>& GetMaterialScaffold() const { return _materialScaffold; }
		const std::string& GetModelScaffoldName() const { return _modelScaffoldName; }
		const std::string& GetMaterialScaffoldName() const { return _materialScaffoldName; }

		SimpleModelRenderer(
			const std::shared_ptr<PipelineAcceleratorPool>& pipelineAcceleratorPool,
			const std::shared_ptr<RenderCore::Assets::ModelScaffold>& modelScaffold,
			const std::shared_ptr<RenderCore::Assets::MaterialScaffold>& materialScaffold,
			IteratorRange<const DeformOperationInstantiation*> deformAttachments = {},
			const std::string& modelScaffoldName = {},
			const std::string& materialScaffoldName = {});
		~SimpleModelRenderer();

		SimpleModelRenderer& operator=(const SimpleModelRenderer&) = delete;
		SimpleModelRenderer(const SimpleModelRenderer&) = delete;
		
		static void ConstructToFuture(
			::Assets::AssetFuture<SimpleModelRenderer>& future,
			const std::shared_ptr<PipelineAcceleratorPool>& pipelineAcceleratorPool,
			StringSection<> modelScaffoldName,
			StringSection<> materialScaffoldName,
			StringSection<> deformOperations = {});

		static void ConstructToFuture(
			::Assets::AssetFuture<SimpleModelRenderer>& future,
			const std::shared_ptr<PipelineAcceleratorPool>& pipelineAcceleratorPool,
			StringSection<> modelScaffoldName);

		struct DeformOp;
	private:
		std::shared_ptr<RenderCore::Assets::ModelScaffold> _modelScaffold;
		std::shared_ptr<RenderCore::Assets::MaterialScaffold> _materialScaffold;

		std::unique_ptr<Float4x4[]> _baseTransforms;
		unsigned _baseTransformCount;

		std::vector<std::shared_ptr<DrawableGeo>> _geos;
		std::vector<std::shared_ptr<DrawableGeo>> _boundSkinnedControllers;

		struct GeoCall
		{
			std::shared_ptr<PipelineAccelerator> _pipelineAccelerator;
			::Assets::FuturePtr<DescriptorSetAccelerator> _compiledDescriptorSet;
		};

		std::vector<GeoCall> _geoCalls;
		std::vector<GeoCall> _boundSkinnedControllerGeoCalls;

		RenderCore::Assets::SkeletonBinding _skeletonBinding;

		std::shared_ptr<UniformsStreamInterface> _usi;

		std::vector<DeformOp> _deformOps;

		std::shared_ptr<IResource> _dynVB;

		std::string _modelScaffoldName;
		std::string _materialScaffoldName;

		class GeoCallBuilder;
	};
}}