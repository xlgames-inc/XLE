// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ShadowUniforms.h"
#include "../../Assets/AssetsCore.h"
#include <memory>

namespace RenderCore { namespace Techniques
{
	class IPipelineAcceleratorPool;
	class RenderPassInstance;
	class ParsingContext;
	class FrameBufferPool;
	class AttachmentPool;
	class SequencerConfig;
}}
namespace RenderCore { class IThreadContext; class IDescriptorSet; }
namespace RenderCore { namespace Assets { class PredefinedDescriptorSetLayout; }}

namespace RenderCore { namespace LightingEngine
{
	class IPreparedShadowResult
	{
	public:
		virtual const std::shared_ptr<IDescriptorSet>& GetDescriptorSet() const = 0;
		virtual ~IPreparedShadowResult();
	};

	class ICompiledShadowPreparer
	{
	public:
		virtual Techniques::RenderPassInstance Begin(
			IThreadContext& threadContext, 
			Techniques::ParsingContext& parsingContext,
			const ShadowProjectionDesc& frustum,
			Techniques::FrameBufferPool& shadowGenFrameBufferPool,
			Techniques::AttachmentPool& shadowGenAttachmentPool) = 0;
		virtual void End(
			IThreadContext& threadContext, 
			Techniques::ParsingContext& parsingContext,
			Techniques::RenderPassInstance& rpi,
			IPreparedShadowResult& res) = 0;
		virtual std::pair<std::shared_ptr<Techniques::SequencerConfig>, std::shared_ptr<Techniques::IShaderResourceDelegate>> GetSequencerConfig() = 0;
		virtual std::shared_ptr<IPreparedShadowResult> CreatePreparedShadowResult() = 0;
		~ICompiledShadowPreparer();
	};

	class ShadowGeneratorDesc;
	class SharedTechniqueDelegateBox;
	::Assets::FuturePtr<ICompiledShadowPreparer> CreateCompiledShadowPreparer(
		const ShadowGeneratorDesc& desc, 
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerator,
		const std::shared_ptr<SharedTechniqueDelegateBox>& delegatesBox,
		const std::shared_ptr<RenderCore::Assets::PredefinedDescriptorSetLayout>& descSetLayout);

	class ShadowPreparationOperators
	{
	public:
		struct Operator
		{
			std::shared_ptr<ICompiledShadowPreparer> _preparer;
			ShadowGeneratorDesc _desc;
		};
		std::vector<Operator> _operators;
	};
	::Assets::FuturePtr<ShadowPreparationOperators> CreateShadowPreparationOperators(
		IteratorRange<const ShadowGeneratorDesc*> shadowGenerators, 
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerator,
		const std::shared_ptr<SharedTechniqueDelegateBox>& delegatesBox,
		const std::shared_ptr<RenderCore::Assets::PredefinedDescriptorSetLayout>& descSetLayout);

	/*void ShadowGen_DrawShadowFrustums(
		Metal::DeviceContext& devContext, 
		Techniques::ParsingContext& parserContext,
		const ShadowProjectionDesc& projectionDesc);*/
}}
