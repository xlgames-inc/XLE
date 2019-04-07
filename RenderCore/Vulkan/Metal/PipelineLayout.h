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
    class PipelineLayoutSignatureFile;
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

    class BoundSignatureFile
    {
    public:
		struct DescriptorSet
		{
			VulkanSharedPtr<VkDescriptorSetLayout>	_layout;
			VulkanSharedPtr<VkDescriptorSet>		_blankBindings;
			
			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				DescriptorSetVerboseDescription _blankBindingsDescription;
				std::string _name;
			#endif
		};

        const DescriptorSet*	GetDescriptorSet(uint64_t signatureFile, uint64_t hashName) const;
		void					RegisterSignatureFile(uint64_t hashName, const PipelineLayoutSignatureFile& signatureFile);

        BoundSignatureFile(ObjectFactory& objectFactory, GlobalPools& globalPools, VkShaderStageFlags stageFlags);
        ~BoundSignatureFile();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

	class PipelineLayoutShaderConfig
	{
	public:
		class DescriptorSet
		{
		public:
			BoundSignatureFile::DescriptorSet			_bound;
			std::shared_ptr<DescriptorSetSignature>		_signature;
			unsigned									_pipelineLayoutBindingIndex;
			// RootSignature::DescriptorSetType			_type;
			unsigned _type;
			unsigned									_uniformStream;
			std::string									_name;
		};
		std::vector<DescriptorSet>					_descriptorSets;
		std::vector<PushConstantsRangeSigniture>	_pushConstants;
		std::shared_ptr<LegacyRegisterBinding>		_legacyRegisterBinding;

		mutable VulkanUniquePtr<VkPipelineLayout>	_cachedPipelineLayout;
		mutable unsigned							_cachedPipelineLayoutId = 0;
		mutable unsigned							_cachedDescriptorSetCount = 0;

		PipelineLayoutShaderConfig();
		PipelineLayoutShaderConfig(ObjectFactory& factory, const PipelineLayoutSignatureFile& signatureFile, uint64_t boundId, PipelineType pipelineType);
		~PipelineLayoutShaderConfig();

		PipelineLayoutShaderConfig& operator=(PipelineLayoutShaderConfig&& moveFrom) = default;
		PipelineLayoutShaderConfig(PipelineLayoutShaderConfig&& moveFrom) = default;
	};

	class PipelineLayoutBuilder
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
		void				SetShaderBasedDescriptorSets(const PipelineLayoutShaderConfig&);

		unsigned			_shaderStageMask = 0u;
		ObjectFactory*		_factory = nullptr;

		PipelineLayoutBuilder();
		~PipelineLayoutBuilder();
	private:
		VkPipelineLayout	_pipelineLayout;
		unsigned			_descriptorSetCount = 0u;
		unsigned			_pipelineLayoutId = 1u;
	};

	VulkanUniquePtr<VkDescriptorSetLayout> CreateDescriptorSetLayout(
        const ObjectFactory& factory, 
        const DescriptorSetSignature& srcLayout,
        VkShaderStageFlags stageFlags);

	#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
		class LegacyRegisterBinding;
		class DescriptorSetVerboseDescription;
		std::ostream& WriteDescriptorSet(
			std::ostream& stream,
			const DescriptorSetVerboseDescription& bindingDescription,
			const DescriptorSetSignature& signature,
			const LegacyRegisterBinding& legacyRegisterBinding,
			IteratorRange<const CompiledShaderByteCode**> compiledShaderByteCode,
			unsigned descriptorSetIndex, bool isBound);
	#endif

	VkDescriptorType AsVkDescriptorType(DescriptorType type);
	const char* AsString(DescriptorType type);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class VulkanGlobalsTemp
	{
	public:
		std::shared_ptr<PipelineLayoutSignatureFile> _graphicsRootSignatureFile;
		std::shared_ptr<PipelineLayoutSignatureFile> _computeRootSignatureFile;

		GlobalPools* _globalPools;

		static VulkanGlobalsTemp& GetInstance();

		std::shared_ptr<BoundSignatureFile> _boundGraphicsSignatures;
		std::shared_ptr<BoundSignatureFile> _boundComputeSignatures;

		static const unsigned s_mainSignature = 1;

		std::shared_ptr<PipelineLayoutShaderConfig> _mainGraphicsConfig;
		std::shared_ptr<PipelineLayoutShaderConfig> _mainComputeConfig;

		VulkanGlobalsTemp();
		~VulkanGlobalsTemp();
	};

}}

