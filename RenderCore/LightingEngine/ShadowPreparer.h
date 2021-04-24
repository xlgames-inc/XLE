// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ShadowUniforms.h"
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
namespace RenderCore { class IThreadContext; }

namespace RenderCore { namespace LightingEngine
{
	class ICompiledShadowPreparer
	{
	public:
		virtual Techniques::RenderPassInstance Begin(
			IThreadContext& threadContext, 
			Techniques::ParsingContext& parsingContext,
			const ShadowProjectionDesc& frustum,
			Techniques::FrameBufferPool& shadowGenFrameBufferPool,
			Techniques::AttachmentPool& shadowGenAttachmentPool) = 0;
		virtual PreparedShadowFrustum End(
			IThreadContext& threadContext, 
			Techniques::ParsingContext& parsingContext,
			Techniques::RenderPassInstance& rpi) = 0;
		virtual std::shared_ptr<Techniques::SequencerConfig> GetSequencerConfig() = 0;
		~ICompiledShadowPreparer();
	};

	class ShadowGeneratorDesc;
	class SharedTechniqueDelegateBox;
	std::shared_ptr<ICompiledShadowPreparer> CreateCompiledShadowPreparer(
		const ShadowGeneratorDesc& desc, 
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerator,
		const std::shared_ptr<SharedTechniqueDelegateBox>& delegatesBox);

	/*void ShadowGen_DrawShadowFrustums(
		Metal::DeviceContext& devContext, 
		Techniques::ParsingContext& parserContext,
		const ShadowProjectionDesc& projectionDesc);*/
}}
