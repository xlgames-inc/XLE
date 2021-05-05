// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderStepFragments.h"

namespace RenderCore { namespace LightingEngine 
{

	RenderCore::AttachmentName RenderStepFragmentInterface::DefineAttachment(uint64_t semantic, const RenderCore::AttachmentDesc& request)
	{
		return _frameBufferDescFragment.DefineAttachment(semantic, request);
	}

	RenderCore::AttachmentName RenderStepFragmentInterface::DefineTemporaryAttachment(const RenderCore::AttachmentDesc& request)
	{
		return _frameBufferDescFragment.DefineTemporaryAttachment(request);
	}

	void RenderStepFragmentInterface::AddSubpass(
		RenderCore::SubpassDesc&& subpass,
		const std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate>& techniqueDelegate,
		Techniques::BatchFilter batchFilter,
		ParameterBox&& sequencerSelectors,
		std::shared_ptr<Techniques::IShaderResourceDelegate> shaderResourceDelegate)
	{
		_frameBufferDescFragment.AddSubpass(std::move(subpass));
		_subpassExtensions.emplace_back(
			SubpassExtension {
				techniqueDelegate, std::move(sequencerSelectors), batchFilter,
				std::move(shaderResourceDelegate)
			});
	}

	RenderStepFragmentInterface::RenderStepFragmentInterface(RenderCore::PipelineType pipelineType)
	{
		_frameBufferDescFragment._pipelineType = pipelineType;
	}

	RenderStepFragmentInterface::~RenderStepFragmentInterface() {}


	const RenderCore::Techniques::SequencerConfig* RenderStepFragmentInstance::GetSequencerConfig() const
	{
		if ((_rpi->GetCurrentSubpassIndex()-_firstSubpassIndex) >= _sequencerConfigs.size())
			return nullptr;
		return _sequencerConfigs[_rpi->GetCurrentSubpassIndex()-_firstSubpassIndex].get();
	}

	RenderStepFragmentInstance::RenderStepFragmentInstance(
		RenderCore::Techniques::RenderPassInstance& rpi,
		IteratorRange<const std::shared_ptr<RenderCore::Techniques::SequencerConfig>*> sequencerConfigs)
	: _rpi(&rpi)
	{
		_firstSubpassIndex = _rpi->GetCurrentSubpassIndex();
		_sequencerConfigs = sequencerConfigs;
	}

	RenderStepFragmentInstance::RenderStepFragmentInstance() {}

}}
