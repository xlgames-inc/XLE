// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "LightingEngine.h"
#include "LightDesc.h"
#include "../Techniques/RenderPass.h"

namespace RenderCore { namespace LightingEngine
{
	class LightingTechniqueIterator;
    class RenderStepFragmentInterface;

	struct FragmentLinkResults
	{
		std::vector<std::pair<uint64_t, AttachmentName>> _generatedAttachments;
		std::vector<uint64_t> _consumedAttachments;
	};

	class CompiledLightingTechnique
	{
	public:
		struct Step
		{
			enum class Type { ParseScene, ExecuteFunction, ExecuteDrawables, BeginRenderPassInstance, EndRenderPassInstance, NextRenderPassStep, None };
			Type _type = Type::None;
			Techniques::BatchFilter _batch = Techniques::BatchFilter::Max;
			std::shared_ptr<Techniques::SequencerConfig> _sequencerConfig;
			std::shared_ptr<Techniques::IShaderResourceDelegate> _shaderResourceDelegate;
			FrameBufferDesc _fbDesc;
			FragmentLinkResults _fragmentLinkResults;

			using FnSig = void(LightingTechniqueIterator&);
			std::function<FnSig> _function;
		};
		std::vector<Step> _steps;

		void Push(std::function<Step::FnSig>&&);
		void PushParseScene(Techniques::BatchFilter);
		void PushExecuteDrawables(
			std::shared_ptr<Techniques::SequencerConfig> sequencerConfig,
			std::shared_ptr<Techniques::IShaderResourceDelegate> uniformDelegate);
		void Push(RenderStepFragmentInterface&& fragmentInterface);

		std::vector<Techniques::PreregisteredAttachment> _workingAttachments;
		FrameBufferProperties _fbProps;

		std::shared_ptr<Techniques::IPipelineAcceleratorPool> _pipelineAccelerators;

		const ::Assets::DependencyValidation& GetDependencyValidation() const { return _depVal; }
		::Assets::DependencyValidation _depVal;

		CompiledLightingTechnique(
			const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
			const FrameBufferProperties& fbProps,
			IteratorRange<const Techniques::PreregisteredAttachment*> preregisteredAttachments);
	};

    class LightingTechniqueIterator
	{
	public:
		Techniques::RenderPassInstance _rpi;
		Techniques::DrawablesPacket _drawablePkt;

		IThreadContext* _threadContext = nullptr;
		Techniques::ParsingContext* _parsingContext = nullptr;
		Techniques::IPipelineAcceleratorPool* _pipelineAcceleratorPool = nullptr;
		Techniques::AttachmentPool* _attachmentPool = nullptr;
		Techniques::FrameBufferPool* _frameBufferPool = nullptr;
		const CompiledLightingTechnique* _compiledTechnique = nullptr;
		SceneLightingDesc _sceneLightingDesc;

		void PushFollowingStep(std::function<CompiledLightingTechnique::Step::FnSig>&& fn);
        void PushFollowingStep(Techniques::BatchFilter batchFilter);
		void PushFollowingStep(std::shared_ptr<Techniques::SequencerConfig> seqConfig, std::shared_ptr<Techniques::IShaderResourceDelegate> uniformDelegate);
		LightingTechniqueIterator(
			IThreadContext& threadContext,
			Techniques::ParsingContext& parsingContext,
			Techniques::IPipelineAcceleratorPool& pipelineAcceleratorPool,
			Techniques::AttachmentPool& attachmentPool,
			Techniques::FrameBufferPool& frameBufferPool,
			const CompiledLightingTechnique& compiledTechnique,
			const SceneLightingDesc& sceneLightingDesc);

	private:
		std::vector<CompiledLightingTechnique::Step> _steps;
		std::vector<CompiledLightingTechnique::Step>::iterator _stepIterator;
		std::vector<CompiledLightingTechnique::Step>::iterator _pushFollowingIterator;
		size_t currentStepIdx = 0;

		friend class LightingTechniqueInstance;
	};

}}

