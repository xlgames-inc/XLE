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

namespace RenderCore { class CompiledShaderByteCode; }

namespace RenderCore { namespace Metal_Vulkan
{
	class ObjectFactory;
	class DescriptorSetSignature;
	class DescriptorSetSignatureFile;
	class RootSignature;
	class LegacyRegisterBinding;
	class PushConstantsRangeSignature;
	class ShaderProgram;
	class ComputeShader;
}}

namespace RenderCore { namespace Metal_Vulkan { namespace Internal
{

#if 0
	class BoundSignatureFile
	{
	public:
		

		const DescriptorSet*	GetDescriptorSet(uint64_t signatureFile, uint64_t hashName) const;
		void					RegisterSignatureFile(uint64_t hashName, const DescriptorSetSignatureFile& signatureFile);

		BoundSignatureFile(ObjectFactory& objectFactory, GlobalPools& globalPools, VkShaderStageFlags stageFlags);
		~BoundSignatureFile();
	private:
		class Pimpl;
		std::unique_ptr<Pimpl> _pimpl;
	};
#endif

	static const unsigned s_maxDescriptorSetCount = 4;

	class PartialPipelineDescriptorsLayout
	{
	public:
		class DescriptorSet
		{
		public:
			std::shared_ptr<DescriptorSetSignature>		_signature;
			unsigned									_pipelineLayoutBindingIndex;
			unsigned 									_type;		// RootSignature::DescriptorSetType
			unsigned									_uniformStream;
			std::string									_name;
		};
		std::vector<DescriptorSet>					_descriptorSets;
		std::vector<PushConstantsRangeSignature>	_pushConstants;
		std::shared_ptr<LegacyRegisterBinding>		_legacyRegisterBinding;
	};

	std::shared_ptr<PartialPipelineDescriptorsLayout> CreatePartialPipelineDescriptorsLayout(
		const DescriptorSetSignatureFile& signatureFile, PipelineType pipelineType);

	VulkanUniquePtr<VkPipelineLayout> CreateVulkanPipelineLayout(
		ObjectFactory& factory,
		CompiledDescriptorSetLayoutCache& cache,
		IteratorRange<const PartialPipelineDescriptorsLayout*> partialLayouts,
		VkShaderStageFlags stageFlags);

	void ValidateRootSignature(
		VkPhysicalDevice physDev,
		const DescriptorSetSignatureFile& signatureFile);

	/*mutable VulkanUniquePtr<VkPipelineLayout>	_cachedPipelineLayout;
	mutable unsigned							_cachedPipelineLayoutId = 0;
	mutable unsigned							_cachedDescriptorSetCount = 0;*/

#if 0
	class PipelineDescriptorsLayoutBuilder
	{
	public:
		struct FixedDescriptorSetLayout
		{
			VulkanSharedPtr<VkDescriptorSetLayout>		_descriptorSet;
			unsigned									_bindingIndex = ~0u;
		};
		FixedDescriptorSetLayout _fixedDescriptorSetLayout[s_maxDescriptorSetCount];

		VkPipelineLayout	GetPipelineLayout() const { return _pipelineLayout; }
		unsigned			GetDescriptorSetCount() const { return _descriptorSetCount; }
		void				SetShaderBasedDescriptorSets(const PartialPipelineDescriptorsLayout&);

		unsigned			_shaderStageMask = 0u;
		ObjectFactory*		_factory = nullptr;

		PipelineDescriptorsLayoutBuilder();
		~PipelineDescriptorsLayoutBuilder();
	private:
		VkPipelineLayout	_pipelineLayout;
		unsigned			_descriptorSetCount = 0u;
		unsigned			_pipelineLayoutId = 1u;
	};
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class VulkanGlobalsTemp
	{
	public:
		std::shared_ptr<DescriptorSetSignatureFile> _graphicsRootSignatureFile;
		std::shared_ptr<DescriptorSetSignatureFile> _computeRootSignatureFile;

		GlobalPools* _globalPools;

		static VulkanGlobalsTemp& GetInstance();

		// std::shared_ptr<BoundSignatureFile> _boundGraphicsSignatures;
		// std::shared_ptr<BoundSignatureFile> _boundComputeSignatures;
		// static const unsigned s_mainSignature = 1;

		// std::shared_ptr<PartialPipelineDescriptorsLayout> _mainGraphicsConfig;
		// std::shared_ptr<PartialPipelineDescriptorsLayout> _mainComputeConfig;

		std::shared_ptr<CompiledDescriptorSetLayoutCache> _compiledDescriptorSetLayoutCache;

		VulkanUniquePtr<VkPipelineLayout> _graphicsPipelineLayout;
		VulkanUniquePtr<VkPipelineLayout> _computePipelineLayout;

		VkPipelineLayout GetPipelineLayout(const ShaderProgram&);
		VkPipelineLayout GetPipelineLayout(const ComputeShader&);

		VulkanGlobalsTemp();
		~VulkanGlobalsTemp();
	};

}}}

