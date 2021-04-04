// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Drawables.h"
#include "PipelineAccelerator.h"
#include "../Types.h"
#include "../StateDesc.h"
#include <memory>

namespace RenderCore { class IThreadContext; class FrameBufferDesc; }
namespace Assets { class IAsyncMarker; }

namespace RenderCore { namespace Techniques
{
	class ParsingContext;

	class IImmediateDrawables
	{
	public:
		virtual IteratorRange<void*> QueueDraw(
			size_t vertexDataSize,
			IteratorRange<const InputElementDesc*> inputAssembly,
			const RenderCore::Assets::RenderStateSet& stateSet,
			Topology topology = Topology::TriangleList,
			const ParameterBox& shaderSelectors = {}) = 0;
		virtual void ExecuteDraws(
			IThreadContext& context,
			ParsingContext& parserContext,
			const FrameBufferDesc& fbDesc,
			unsigned subpassIndex) = 0;
		virtual std::shared_ptr<::Assets::IAsyncMarker> PrepareResources(
			const FrameBufferDesc& fbDesc,
			unsigned subpassIndex) = 0;
		virtual ~IImmediateDrawables();
	};

	std::shared_ptr<IImmediateDrawables> CreateImmediateDrawables(
		const std::shared_ptr<IDevice>&,
		const std::shared_ptr<RenderCore::ICompiledPipelineLayout>&,
		const DescriptorSetLayoutAndBinding& matDescSetLayout = Internal::GetDefaultDescriptorSetLayoutAndBinding(),
		const DescriptorSetLayoutAndBinding& sequencerDescSetLayout = Internal::GetDefaultDescriptorSetLayoutAndBinding());
}}

