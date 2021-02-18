// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanForward.h"
#include "VulkanCore.h"
#include "DescriptorSet.h"		// (for DescriptorSetVerboseDescription)
#include "../../Types.h"
#include "../../../Assets/AssetsCore.h"
#include "../../../Assets/AssetUtils.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

namespace RenderCore { class CompiledShaderByteCode; class ConstantBufferElementDesc; class DescriptorSetSignature; }

namespace RenderCore { namespace Metal_Vulkan
{
	class ObjectFactory;
	class DescriptorSetSignatureFile;
	class RootSignature;
	class LegacyRegisterBinding;
	class PushConstantsRangeSignature;
	class ShaderProgram;
	class ComputeShader;
}}

namespace RenderCore { namespace Metal_Vulkan
{
	static const unsigned s_maxDescriptorSetCount = 4;
	static const unsigned s_maxPushConstantBuffers = 4;

	class CompiledPipelineLayout
	{
	public:
		using DescriptorSetIndex = unsigned;
		using DescriptorSetLayoutPtr = std::shared_ptr<CompiledDescriptorSetLayout>;
		using DescriptorSetPtr = VulkanSharedPtr<VkDescriptorSet>;
		
		VkPipelineLayout				GetUnderlying() const;
		const DescriptorSetLayoutPtr&	GetDescriptorSetLayout(DescriptorSetIndex) const;
		const DescriptorSetPtr&			GetBlankDescriptorSet(DescriptorSetIndex) const;
		uint64_t						GetDescriptorSetBindingName(DescriptorSetIndex) const;
		unsigned 						GetDescriptorSetCount() const;

		#if defined(VULKAN_VERBOSE_DEBUG)
			const DescriptorSetDebugInfo& GetBlankDescriptorSetDebugInfo(DescriptorSetIndex) const;
			void WriteDebugInfo(
				std::ostream&& output,
				IteratorRange<const CompiledShaderByteCode**> shaders,
				IteratorRange<const DescriptorSetDebugInfo*> descriptorSets);
		#endif

		struct DescriptorSetBinding
		{
			std::string _name;
			std::shared_ptr<CompiledDescriptorSetLayout> _layout;
			DescriptorSetPtr _blankDescriptorSet;

			#if defined(VULKAN_VERBOSE_DEBUG)
				DescriptorSetDebugInfo _blankDescriptorSetDebugInfo;
			#endif
		};

		struct PushConstantsBinding
		{
			std::string _name;
			unsigned _cbSize = 0;
			VkShaderStageFlags _stageFlags = 0;
			IteratorRange<const ConstantBufferElementDesc*> _cbElements;
		};

		CompiledPipelineLayout(
			ObjectFactory& factory,
			IteratorRange<const DescriptorSetBinding*> descriptorSets,
			IteratorRange<const PushConstantsBinding*> pushConstants);
	private:
		VulkanUniquePtr<VkPipelineLayout> _pipelineLayout;

		DescriptorSetLayoutPtr	_descriptorSetLayouts[s_maxDescriptorSetCount];
		DescriptorSetPtr		_blankDescriptorSets[s_maxDescriptorSetCount];
		unsigned				_descriptorSetCount;
		unsigned 				_pushConstantBufferCount;

		uint64_t				_descriptorSetBindingNames[s_maxDescriptorSetCount];
		uint64_t				_pushConstantBufferBindingNames[s_maxPushConstantBuffers];

		#if defined(VULKAN_VERBOSE_DEBUG)
			DescriptorSetDebugInfo _blankDescriptorSetsDebugInfo[s_maxDescriptorSetCount];
			std::string _descriptorSetStringNames[s_maxDescriptorSetCount];
		#endif
	};

	inline VkPipelineLayout CompiledPipelineLayout::GetUnderlying() const { return _pipelineLayout.get(); }
		
	inline auto CompiledPipelineLayout::GetDescriptorSetLayout(DescriptorSetIndex binding) const -> const DescriptorSetLayoutPtr&
	{
		assert(binding < _descriptorSetCount);
		return _descriptorSetLayouts[binding];
	}

	inline auto CompiledPipelineLayout::GetBlankDescriptorSet(DescriptorSetIndex binding) const -> const DescriptorSetPtr&
	{
		assert(binding < _descriptorSetCount);
		return _blankDescriptorSets[binding];
	}

	inline uint64_t CompiledPipelineLayout::GetDescriptorSetBindingName(DescriptorSetIndex binding) const
	{
		assert(binding < _descriptorSetCount);
		return _descriptorSetBindingNames[binding];
	}

	inline unsigned CompiledPipelineLayout::GetDescriptorSetCount() const
	{
		return _descriptorSetCount;
	}

	#if defined(VULKAN_VERBOSE_DEBUG)
		inline const DescriptorSetDebugInfo& CompiledPipelineLayout::GetBlankDescriptorSetDebugInfo(DescriptorSetIndex binding) const
		{
			assert(binding < _descriptorSetCount);
			return _blankDescriptorSetsDebugInfo[binding];
		}
	#endif

	namespace Internal
	{
		class PartialPipelineDescriptorsLayout
		{
		public:
			class DescriptorSet
			{
			public:
				std::shared_ptr<DescriptorSetSignature>		_signature;
				unsigned									_pipelineLayoutBindingIndex;
				std::string									_name;
			};
			std::vector<DescriptorSet>					_descriptorSets;
			std::vector<PushConstantsRangeSignature>	_pushConstants;
			std::shared_ptr<LegacyRegisterBinding>		_legacyRegisterBinding;
		};

		std::shared_ptr<PartialPipelineDescriptorsLayout> CreatePartialPipelineDescriptorsLayout(
			const DescriptorSetSignatureFile& signatureFile, PipelineType pipelineType);

		class CompiledDescriptorSetLayoutCache;
		std::shared_ptr<CompiledPipelineLayout> CreateCompiledPipelineLayout(
			ObjectFactory& factory,
			CompiledDescriptorSetLayoutCache& cache,
			IteratorRange<const PartialPipelineDescriptorsLayout*> partialLayouts,
			VkShaderStageFlags stageFlags);

		void ValidateRootSignature(
			VkPhysicalDevice physDev,
			const DescriptorSetSignatureFile& signatureFile);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		std::shared_ptr<CompiledDescriptorSetLayoutCache> CreateCompiledDescriptorSetLayoutCache(ObjectFactory& objectFactory, GlobalPools& globalPools);

		class VulkanGlobalsTemp
		{
		public:
			std::shared_ptr<DescriptorSetSignatureFile> _graphicsRootSignatureFile;
			std::shared_ptr<DescriptorSetSignatureFile> _computeRootSignatureFile;

			GlobalPools* _globalPools;

			static VulkanGlobalsTemp& GetInstance();

				// note -- move this into GlobalPools?
			std::shared_ptr<CompiledDescriptorSetLayoutCache> _compiledDescriptorSetLayoutCache;

			std::shared_ptr<CompiledPipelineLayout> _graphicsPipelineLayout;
			std::shared_ptr<CompiledPipelineLayout> _computePipelineLayout;

			const std::shared_ptr<CompiledPipelineLayout>& GetPipelineLayout(const ShaderProgram&);
			const std::shared_ptr<CompiledPipelineLayout>& GetPipelineLayout(const ComputeShader&);

			const LegacyRegisterBinding& GetLegacyRegisterBinding();

			unsigned _graphicsUniformStreamToDescriptorSetBinding[4];
			unsigned _computeUniformStreamToDescriptorSetBinding[4];

			VulkanGlobalsTemp();
			~VulkanGlobalsTemp();
		};
	}
}}

