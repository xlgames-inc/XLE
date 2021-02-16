// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanCore.h"
#include "../../UniformsStream.h"
#include "../../Types.h"		// for PipelineType
#include "../../../Utility/IteratorUtils.h"
#include <memory>
#include <vector>

namespace RenderCore
{
	class UniformsStream;
	class UniformsStreamInterface;
	// class VertexBufferView;
	class InputElementDesc;
	class MiniInputElementDesc;
	class CompiledShaderByteCode;
	template <typename Type, int Count> class ResourceList;
}

namespace RenderCore { namespace Metal_Vulkan { namespace Internal {
	class PartialPipelineDescriptorsLayout;
}}}

namespace RenderCore { namespace Metal_Vulkan
{
	class ShaderProgram;
	class DeviceContext;
	class ComputeShader;
	class SPIRVReflection;

		////////////////////////////////////////////////////////////////////////////////////////////////

	class BoundInputLayout
	{
	public:
		bool AllAttributesBound() const { return _allAttributesBound; }

		BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const CompiledShaderByteCode& shader);
		BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const ShaderProgram& shader);
		
		struct SlotBinding
		{
			IteratorRange<const MiniInputElementDesc*> _elements;
			unsigned _instanceStepDataRate;     // set to 0 for per vertex, otherwise a per-instance rate
		};
		BoundInputLayout(
			IteratorRange<const SlotBinding*> layouts,
			const CompiledShaderByteCode& program);
		BoundInputLayout(
			IteratorRange<const SlotBinding*> layouts,
			const ShaderProgram& shader);

		BoundInputLayout();
		~BoundInputLayout();

		BoundInputLayout(BoundInputLayout&& moveFrom) never_throws = default;
		BoundInputLayout& operator=(BoundInputLayout&& moveFrom) never_throws = default;

		const IteratorRange<const VkVertexInputAttributeDescription*> GetAttributes() const { return MakeIteratorRange(_attributes); }
		const IteratorRange<const VkVertexInputBindingDescription*> GetVBBindings() const { return MakeIteratorRange(_vbBindingDescriptions); }
		uint64_t GetPipelineRelevantHash() const { return _pipelineRelevantHash; }
	private:
		std::vector<VkVertexInputAttributeDescription>	_attributes;
		std::vector<VkVertexInputBindingDescription>	_vbBindingDescriptions;
		uint64_t _pipelineRelevantHash;
		bool _allAttributesBound;

		void CalculateAllAttributesBound(const SPIRVReflection& reflection);
	};

		////////////////////////////////////////////////////////////////////////////////////////////////

	class BoundPipelineLayout;
	class SharedGraphicsEncoder;
	class PipelineLayoutConfig;
	class GraphicsPipeline;
	class ComputePipeline;
	class DescriptorSetSignature;

	namespace Internal { class VulkanPipelineLayout; }

	class BoundUniforms
	{
	public:
		void ApplyLooseUniforms(
			DeviceContext& context,
			SharedGraphicsEncoder& encoder,
			const UniformsStream& stream) const;
		void UnbindLooseUniforms(DeviceContext& context, SharedGraphicsEncoder& encoder);

		void ApplyDescriptorSets(
			DeviceContext& context,
			SharedGraphicsEncoder& encoder,
			IteratorRange<const DescriptorSet**> descriptorSets);

		uint64_t GetBoundLooseConstantBuffers() const { return _boundLooseUniformBuffers; }
		uint64_t GetBoundLooseResources() const { return _boundLooseResources; }
		uint64_t GetBoundLooseSamplers() const { return _boundLooseSamplerStates; }

		struct DescriptorSetBinding
		{
		public:
			uint64_t _bindingName = 0;
			const RenderCore::DescriptorSetSignature* _layout = nullptr;
		};

		BoundUniforms(
			const ShaderProgram& shader,
			IteratorRange<const DescriptorSetBinding*> descriptorSetBindings,
			const UniformsStreamInterface& looseUniforms = {});
		BoundUniforms(
			const ComputeShader& shader,
			IteratorRange<const DescriptorSetBinding*> descriptorSetBindings,
			const UniformsStreamInterface& looseUniforms = {});
		BoundUniforms(
			const GraphicsPipeline& shader,
			IteratorRange<const DescriptorSetBinding*> descriptorSetBindings,
			const UniformsStreamInterface& looseUniforms = {});
		BoundUniforms(
			const ComputePipeline& shader,
			IteratorRange<const DescriptorSetBinding*> descriptorSetBindings,
			const UniformsStreamInterface& looseUniforms = {});
		BoundUniforms();
		~BoundUniforms();
		BoundUniforms(const BoundUniforms&) = default;
		BoundUniforms& operator=(const BoundUniforms&) = default;
		BoundUniforms(BoundUniforms&&) = default;
		BoundUniforms& operator=(BoundUniforms&&) = default;

	private:
		struct LooseUniformBind { uint32_t _descSetSlot; uint32_t _inputUniformStreamIdx; };
		struct AdaptiveSetBindingRules
		{
			uint32_t _descriptorSetIdx = 0u;
			uint32_t _shaderStageMask = 0u;
			VulkanSharedPtr<VkDescriptorSetLayout> _underlyingLayout;

			std::vector<LooseUniformBind> _cbBinds;
			std::vector<LooseUniformBind> _srvBinds;
			std::vector<LooseUniformBind> _samplerBinds;
			
			// these exist so we default out slots that are used by the shader, but not provided as input
			std::shared_ptr<DescriptorSetSignature> _sig;
			uint64_t _shaderUsageMask = 0ull;
		};
		std::vector<AdaptiveSetBindingRules> _adaptiveSetRules;

		struct PushConstantBindingRules
		{
			uint32_t	_shaderStageBind;
			unsigned	_inputCBSlot;
		};
		std::vector<PushConstantBindingRules> _pushConstantsRules;

		struct FixedDescriptorSetBindingRules
		{
			uint32_t _inputSlot;
			uint32_t _outputSlot;
			uint32_t _shaderStageMask;
		};
		std::vector<FixedDescriptorSetBindingRules> _fixedDescriptorSetRules;
		
		PipelineType _pipelineType;
		uint64_t _boundLooseUniformBuffers;
		uint64_t _boundLooseResources;
		uint64_t _boundLooseSamplerStates;

		class ConstructionHelper;
		class BindingHelper;

		#if defined(_DEBUG)
			std::string _debuggingDescription;
		#endif
	};

		////////////////////////////////////////////////////////////////////////////////////////////////

	class ShaderResourceView;
	class UnorderedAccessView;
	class SamplerState;
	class Buffer;
	using ConstantBuffer = Buffer;
	class TextureView;
	class ObjectFactory;
	class DescriptorSetSignature;
	class DescriptorSetDebugInfo;
	class GlobalPools;
	class LegacyRegisterBinding;

	/// <summary>Bind uniforms at numeric binding points</summary>
	class NumericUniformsInterface
	{
	public:
		void    BindSRV(unsigned startingPoint, IteratorRange<const TextureView*const*> resources);
		void    BindUAV(unsigned startingPoint, IteratorRange<const TextureView*const*> resources);
		void    BindCB(unsigned startingPoint, IteratorRange<const VkBuffer*> uniformBuffers);
		void    BindSampler(unsigned startingPoint, IteratorRange<const VkSampler*> samplers);

		void    GetDescriptorSets(
			IteratorRange<VkDescriptorSet*> dst
			VULKAN_VERBOSE_DEBUG_ONLY(, IteratorRange<DescriptorSetDebugInfo**> descriptions));
		void    Reset();
		bool	HasChanges() const;
		
		template<int Count> void Bind(const ResourceList<ShaderResourceView, Count>&);
		template<int Count> void Bind(const ResourceList<SamplerState, Count>&);
		template<int Count> void Bind(const ResourceList<ConstantBuffer, Count>&);
		template<int Count> void Bind(const ResourceList<UnorderedAccessView, Count>&);

		const DescriptorSetSignature& GetSignature() const;
		const LegacyRegisterBinding& GetLegacyRegisterBindings() const;

		NumericUniformsInterface(
			const ObjectFactory& factory,
			GlobalPools& globalPools,
			const std::shared_ptr<DescriptorSetSignature>& signature,
			const LegacyRegisterBinding& bindings,
			VkShaderStageFlags stageFlags,
			unsigned descriptorSetIndex);
		NumericUniformsInterface();
		~NumericUniformsInterface();

		NumericUniformsInterface(NumericUniformsInterface&&);
		NumericUniformsInterface& operator=(NumericUniformsInterface&&);
	protected:
		class Pimpl;
		std::unique_ptr<Pimpl> _pimpl;
	};

		////////////////////////////////////////////////////////////////////////////////////////////////

	template<int Count> 
		void    NumericUniformsInterface::Bind(const ResourceList<ShaderResourceView, Count>& shaderResources) 
		{
			auto r = MakeIteratorRange(shaderResources._buffers);
			BindSRV(
				shaderResources._startingPoint,
				MakeIteratorRange((const TextureView*const*)r.begin(), (const TextureView*const*)r.end()));
		}
	
	template<int Count> void    NumericUniformsInterface::Bind(const ResourceList<SamplerState, Count>& samplerStates) 
		{
			VkSampler samplers[Count];
			for (unsigned c=0; c<Count; ++c)
				samplers[c] = samplerStates._buffers[c]->GetUnderlying();
			BindSampler(
				samplerStates._startingPoint,
				MakeIteratorRange(samplers));
		}
	
	template<int Count> 
		void    NumericUniformsInterface::Bind(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
		{
			VkBuffer buffers[Count];
			for (unsigned c=0; c<Count; ++c)
				buffers[c] = constantBuffers._buffers[c]->GetBuffer();
			BindCB(
				constantBuffers._startingPoint,
				MakeIteratorRange(buffers));
		}

	template<int Count> 
		void    NumericUniformsInterface::Bind(const ResourceList<UnorderedAccessView, Count>& uavs)
		{
			auto r = MakeIteratorRange(uavs._buffers);
			BindUAV(
				uavs._startingPoint,
				MakeIteratorRange((const TextureView*const*)r.begin(), (const TextureView*const*)r.end()));
		}

}}

