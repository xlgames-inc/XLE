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
	class BoundPipelineLayout;
	class LegacyRegisterBinding;
	class PushConstantsRangeSigniture;

	enum class DescriptorType
    {
        Sampler,
        Texture,
        ConstantBuffer,
        UnorderedAccessTexture,
        UnorderedAccessBuffer,
        Unknown
    };

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

	struct CompiledDescriptorSetLayout
	{
		VulkanSharedPtr<VkDescriptorSetLayout>	_layout;
		VulkanSharedPtr<VkDescriptorSet>		_blankBindings;
		
		#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
			DescriptorSetVerboseDescription _blankBindingsDescription;
			std::string _name;
		#endif
	};

	class PartialPipelineDescriptorsLayout
	{
	public:
		class DescriptorSet
		{
		public:
			CompiledDescriptorSetLayout					_bound;
			std::shared_ptr<DescriptorSetSignature>		_signature;
			unsigned									_pipelineLayoutBindingIndex;
			unsigned 									_type;		// RootSignature::DescriptorSetType
			unsigned									_uniformStream;
			std::string									_name;
		};
		std::vector<DescriptorSet>					_descriptorSets;
		std::vector<PushConstantsRangeSigniture>	_pushConstants;
		std::shared_ptr<LegacyRegisterBinding>		_legacyRegisterBinding;

		PartialPipelineDescriptorsLayout();
		~PartialPipelineDescriptorsLayout();

		PartialPipelineDescriptorsLayout& operator=(PartialPipelineDescriptorsLayout&& moveFrom) = default;
		PartialPipelineDescriptorsLayout(PartialPipelineDescriptorsLayout&& moveFrom) = default;
	};

	std::shared_ptr<PartialPipelineDescriptorsLayout>  CreatePartialPipelineDescriptorsLayout(
		ObjectFactory& factory,
		const DescriptorSetSignatureFile& signatureFile, uint64_t boundId, PipelineType pipelineType);

	VulkanUniquePtr<VkPipelineLayout> CreateVulkanPipelineLayout(
		ObjectFactory& factory,
		IteratorRange<const PartialPipelineDescriptorsLayout*> partialLayouts);

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
		static const unsigned s_maxDescriptorSetCount = 4;
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

	VulkanUniquePtr<VkDescriptorSetLayout> CreateDescriptorSetLayout(
        const ObjectFactory& factory, 
        const DescriptorSetSignature& srcLayout,
        VkShaderStageFlags stageFlags);

	#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
		class LegacyRegisterBinding;
		class DescriptorSetVerboseDescription;
		std::ostream& WriteDescriptorSet(
			std::ostream&& stream,
			const DescriptorSetVerboseDescription& bindingDescription,
			const DescriptorSetSignature& signature,
			const LegacyRegisterBinding& legacyRegisterBinding,
			IteratorRange<const CompiledShaderByteCode**> compiledShaderByteCode,
			unsigned descriptorSetIndex, bool isBound);
	#endif

	VkDescriptorType_ AsVkDescriptorType(DescriptorType type);
	const char* AsString(DescriptorType type);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class ShaderProgram;
	class ComputeShader;

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

		std::shared_ptr<PartialPipelineDescriptorsLayout> _mainGraphicsConfig;
		std::shared_ptr<PartialPipelineDescriptorsLayout> _mainComputeConfig;

		VkPipelineLayout GetPipelineLayout(const ShaderProgram&);
		VkPipelineLayout GetPipelineLayout(const ComputeShader&);

		VulkanGlobalsTemp();
		~VulkanGlobalsTemp();
	};

}}

