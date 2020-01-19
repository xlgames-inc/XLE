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

	class PipelineAcceleratorPool
	{
	public:
		std::shared_ptr<PipelineAccelerator> CreatePipelineAccelerator(
			const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
			const ParameterBox& materialSelectors,
			IteratorRange<const InputElementDesc*> inputAssembly,
			RenderCore::Topology topology,
			const RenderCore::Assets::RenderStateSet& stateSet);

		std::shared_ptr<SequencerConfig> CreateSequencerConfig(
			const std::shared_ptr<ITechniqueDelegate>& delegate,
			const ParameterBox& sequencerSelectors,
			const FrameBufferDesc& fbDesc,
			unsigned subpassIndex = 0);

		const ::Assets::FuturePtr<Metal::GraphicsPipeline>& GetPipeline(PipelineAccelerator& pipelineAccelerator, const SequencerConfig& sequencerConfig) const;
		const Metal::GraphicsPipeline* TryGetPipeline(PipelineAccelerator& pipelineAccelerator, const SequencerConfig& sequencerConfig) const;

		void			SetGlobalSelector(StringSection<> name, IteratorRange<const void*> data, const ImpliedTyping::TypeDesc& type);
		T1(Type) void   SetGlobalSelector(StringSection<> name, Type value);
		void			RemoveGlobalSelector(StringSection<> name);

		void			SetFrameBufferProperties(const FrameBufferProperties& fbProps);

		unsigned		GetGUID() const { return _guid; }

		void			RebuildAllOutOfDatePipelines();

		PipelineAcceleratorPool();
		~PipelineAcceleratorPool();
		PipelineAcceleratorPool(const PipelineAcceleratorPool&) = delete;
		PipelineAcceleratorPool& operator=(const PipelineAcceleratorPool&) = delete;

		uint64_t GetHash() const { return _guid; }	// GetHash() function for Assets::Internal::HashParam expansion
	private:
		class Pimpl;
		std::unique_ptr<Pimpl> _pimpl;
		unsigned _guid;
	};

	T1(Type) inline void   PipelineAcceleratorPool::SetGlobalSelector(StringSection<> name, Type value)
	{
		const auto insertType = ImpliedTyping::TypeOf<Type>();
        auto size = insertType.GetSize();
        assert(size == sizeof(Type)); (void)size;
        SetGlobalSelector(name, AsOpaqueIteratorRange(value), insertType);
	}

	std::shared_ptr<PipelineAcceleratorPool> CreatePipelineAcceleratorPool();
}}

