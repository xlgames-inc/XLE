// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../UniformsStream.h"
#include "../IDevice_Forward.h"
#include "../Metal/Forward.h"
#include "../../Assets/AssetFuture.h"
#include <vector>
#include <memory>

namespace RenderCore { namespace Assets { class PredefinedDescriptorSetLayout; }}
namespace RenderCore { class IResource; class IDevice; }
namespace Utility { class ParameterBox; }

namespace RenderCore { namespace Techniques 
{
	class DeferredShaderResource;

	class LegacyDescriptorSetAccelerator
	{
	public:
		UniformsStreamInterface _usi;

		std::vector<std::shared_ptr<DeferredShaderResource>> _shaderResources;
		std::vector<std::shared_ptr<ISampler>> _samplerStates;
		std::vector<std::shared_ptr<IResourceView>> _constantBuffers;

		::Assets::DepValPtr _depVal;

		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

		void Apply(
			Metal::DeviceContext& devContext,
			Metal::GraphicsEncoder& encoder,
			Metal::BoundUniforms& boundUniforms) const;
	};

	void ConstructDescriptorSet(
		::Assets::AssetFuture<RenderCore::IDescriptorSet>& future,
		const std::shared_ptr<IDevice>& device,
		const Utility::ParameterBox& constantBindings,
		const Utility::ParameterBox& resourceBindings,
		IteratorRange<const std::pair<uint64_t, std::shared_ptr<ISampler>>*> samplerBindings,
		const RenderCore::Assets::PredefinedDescriptorSetLayout& layout);

}}
