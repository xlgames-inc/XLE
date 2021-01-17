// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Metal/Forward.h"		// (for Metal::GraphicsPipeline)
#include "../Types.h"
#include "../../Assets/AssetsCore.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Utility/StringUtils.h"
#include <memory>

namespace RenderCore 
{
	class FrameBufferDesc;
	class FrameBufferProperties;
	class InputElementDesc;
}

namespace RenderCore { namespace Assets { class RenderStateSet; } }

namespace RenderCore { namespace Techniques
{
	class CompiledShaderPatchCollection;
	class PipelineAccelerator;
	class ITechniqueDelegate;
	class SequencerConfig;

	// Switching this to a virtual interface style class in order to better support multiple DLLs/modules
	// For many objects like the SimpleModelRenderer, the pipeline accelerator pools is one of the main
	// interfaces for interacting with render states and shaders. By making this an interface class, it
	// allows us to keep the implementation for the pool in the main host module, even if DLLs have their
	// own SimpleModelRenderer, etc
	class IPipelineAcceleratorPool
	{
	public:
		virtual std::shared_ptr<PipelineAccelerator> CreatePipelineAccelerator(
			const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
			const ParameterBox& materialSelectors,
			IteratorRange<const InputElementDesc*> inputAssembly,
			RenderCore::Topology topology,
			const RenderCore::Assets::RenderStateSet& stateSet) = 0;

		virtual std::shared_ptr<SequencerConfig> CreateSequencerConfig(
			const std::shared_ptr<ITechniqueDelegate>& delegate,
			const ParameterBox& sequencerSelectors,
			const FrameBufferDesc& fbDesc,
			unsigned subpassIndex = 0) = 0;

		virtual const ::Assets::FuturePtr<Metal::GraphicsPipeline>& GetPipeline(PipelineAccelerator& pipelineAccelerator, const SequencerConfig& sequencerConfig) const = 0;
		virtual const Metal::GraphicsPipeline* TryGetPipeline(PipelineAccelerator& pipelineAccelerator, const SequencerConfig& sequencerConfig) const = 0;

		virtual void	SetGlobalSelector(StringSection<> name, IteratorRange<const void*> data, const ImpliedTyping::TypeDesc& type) = 0;
		T1(Type) void   SetGlobalSelector(StringSection<> name, Type value);
		virtual void	RemoveGlobalSelector(StringSection<> name) = 0;

		virtual void	SetFrameBufferProperties(const FrameBufferProperties& fbProps) = 0;
		virtual void	RebuildAllOutOfDatePipelines() = 0;

		virtual ~IPipelineAcceleratorPool();

		unsigned GetGUID() const { return _guid; }
		uint64_t GetHash() const { return _guid; }	// GetHash() function for Assets::Internal::HashParam expansion
	protected:
		unsigned _guid;
	};

	T1(Type) inline void   IPipelineAcceleratorPool::SetGlobalSelector(StringSection<> name, Type value)
	{
		const auto insertType = ImpliedTyping::TypeOf<Type>();
        auto size = insertType.GetSize();
        assert(size == sizeof(Type)); (void)size;
        SetGlobalSelector(name, MakeOpaqueIteratorRange(value), insertType);
	}

	std::shared_ptr<IPipelineAcceleratorPool> CreatePipelineAcceleratorPool();
}}

