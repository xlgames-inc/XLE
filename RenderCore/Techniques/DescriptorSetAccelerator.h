// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../UniformsStream.h"
#include "../Metal/Forward.h"
#include "../../Assets/AssetFuture.h"
#include <vector>
#include <memory>

namespace RenderCore { namespace Assets { class PredefinedDescriptorSetLayout; }}
namespace RenderCore { class IResource; }
namespace Utility { class ParameterBox; }

namespace RenderCore { namespace Techniques 
{
	class DeferredShaderResource;

	class DescriptorSetAccelerator
	{
	public:
		UniformsStreamInterface _usi;

		std::vector<std::shared_ptr<DeferredShaderResource>> _shaderResources;
		std::vector<Metal::SamplerState> _samplerStates;
		std::vector<std::shared_ptr<IResource>> _constantBuffers;

		::Assets::DepValPtr _depVal;

		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

		void Apply(
			Metal::DeviceContext& devContext,
			Metal::BoundUniforms& boundUniforms,
			unsigned streamIdx) const;
	};

	::Assets::FuturePtr<DescriptorSetAccelerator> MakeDescriptorSetAccelerator(
		const Utility::ParameterBox& constantBindings,
		const Utility::ParameterBox& resourceBindings,
		const RenderCore::Assets::PredefinedDescriptorSetLayout& layout,
		const std::string& descriptorSetName);

}}
