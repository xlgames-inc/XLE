// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Metal/Forward.h"		// (for Metal::GraphicsPipeline)
#include "../Types.h"
#include "../../Assets/AssetFuture.h"
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
	class ShaderSelectors;
	class CompiledShaderPatchCollection;
	class PipelineAccelerator;
	class ITechniqueDelegate_New;

	using SequencerConfigId = uint64_t;

	class PipelineAccelerator
	{
	public:
		const ::Assets::FuturePtr<Metal::GraphicsPipeline>& GetPipeline(SequencerConfigId cfgId) const;
		const Metal::GraphicsPipeline* TryGetPipeline(SequencerConfigId cfgId) const;
	};

	class PipelineAcceleratorPool
	{
	public:
		std::shared_ptr<PipelineAccelerator> CreatePipelineAccelerator(
			const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
			const ParameterBox& materialSelectors,
			IteratorRange<const InputElementDesc*> inputAssembly,
			RenderCore::Topology topology,
			const RenderCore::Assets::RenderStateSet& stateSet);

		SequencerConfigId CreateSequencerConfig(
			const std::shared_ptr<ITechniqueDelegate_New>& delegate,
			const ParameterBox& sequencerSelectors,
			const FrameBufferProperties& fbProps,
			const FrameBufferDesc& fbDesc,
			unsigned subpassIndex = 0);

		void			SetGlobalSelector(StringSection<> name, IteratorRange<const void*> data, const ImpliedTyping::TypeDesc& type);
		T1(Type) void   SetGlobalSelector(StringSection<> name, Type value);
		void			RemoveGlobalSelector(StringSection<> name);

		unsigned		GetGUID() const { return _guid; }

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

