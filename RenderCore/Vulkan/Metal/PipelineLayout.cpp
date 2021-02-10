// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PipelineLayout.h"
#include "DescriptorSet.h"
#include "DescriptorSetSignatureFile.h"
#include "ObjectFactory.h"
#include "Pools.h"
#include "DescriptorSetSignatureFile.h"
#include "IncludeVulkan.h"
#include "../../../Utility/Threading/Mutex.h"
#include "../../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Metal_Vulkan { namespace Internal
{
#if 0
	class BoundSignatureFile::Pimpl
	{
	public:
		std::vector<std::pair<uint64_t, DescriptorSet>> _descriptorSetLayouts;
		VkShaderStageFlags								_stageFlags;
		ObjectFactory*		_factory;
		GlobalPools*		_globalPools;
	};

	auto BoundSignatureFile::GetDescriptorSet(uint64_t signatureFile, uint64_t hashName) const -> const DescriptorSet*
	{
		auto h = HashCombine(signatureFile, hashName);
		auto i = LowerBound(_pimpl->_descriptorSetLayouts, h);
		if (i != _pimpl->_descriptorSetLayouts.end() && i->first == h)
			return &i->second;
		return nullptr;
	}

	void BoundSignatureFile::RegisterSignatureFile(uint64_t hashName, const DescriptorSetSignatureFile& signatureFile)
	{
		// Each descriptor set layout is initialized from the root signature
		// This allows us to create a single global setting that can be used broadly across
		// many "pipelines"

		#if defined(_DEBUG)
			ValidateRootSignature(_pimpl->_factory->GetPhysicalDevice(), signatureFile);
		#endif

		_pimpl->_descriptorSetLayouts.reserve(_pimpl->_descriptorSetLayouts.size() + signatureFile._descriptorSets.size());

		for (const auto& s:signatureFile._descriptorSets) {
			DescriptorSet ds;
			ds._layout = CreateDescriptorSetLayout(*_pimpl->_factory, *s, _pimpl->_stageFlags);
			
			{
				DescriptorSetBuilder builder(*_pimpl->_globalPools);
				builder.BindDummyDescriptors(*s, (1ull<<uint64_t(s->_bindings.size()))-1ull);
				ds._blankBindings = _pimpl->_globalPools->_longTermDescriptorPool.Allocate(ds._layout.get());
				VULKAN_VERBOSE_DESCRIPTIONS_ONLY(ds._blankBindingsDescription._descriptorSetInfo = s_dummyDescriptorSetName);
				builder.FlushChanges(
					_pimpl->_factory->GetDevice().get(),
					ds._blankBindings.get(),
					0, 0 VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, ds._blankBindingsDescription));
			}

			VULKAN_VERBOSE_DESCRIPTIONS_ONLY(ds._name = s->_name);
			_pimpl->_descriptorSetLayouts.emplace_back(std::make_pair(HashCombine(hashName, s->_hashName), std::move(ds)));
		}
		std::sort(
			_pimpl->_descriptorSetLayouts.begin(),
			_pimpl->_descriptorSetLayouts.end(),
			CompareFirst<uint64_t, DescriptorSet>());
	}

	BoundSignatureFile::BoundSignatureFile(ObjectFactory& objectFactory, GlobalPools& globalPools, VkShaderStageFlags stageFlags)
	{
		_pimpl = std::make_unique<Pimpl>();
		_pimpl->_stageFlags = stageFlags;
		_pimpl->_factory = &objectFactory;
		_pimpl->_globalPools = &globalPools;
	}

	BoundSignatureFile::~BoundSignatureFile()
	{}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////

	VulkanUniquePtr<VkPipelineLayout> CreateVulkanPipelineLayout(
		ObjectFactory& factory,
		CompiledDescriptorSetLayoutCache& cache,
		IteratorRange<const PartialPipelineDescriptorsLayout*> partialLayouts,
		VkShaderStageFlags stageFlags)
	{
		VkDescriptorSetLayout rawDescriptorSetLayouts[s_maxDescriptorSetCount] = {};
		unsigned descriptorSetCount = 0;
		std::vector<VkPushConstantRange> rawPushConstantRanges;

		for (const auto&partial:partialLayouts) {
			for (auto& desc:partial._descriptorSets) {
				assert(desc._pipelineLayoutBindingIndex < s_maxDescriptorSetCount);
				assert(!rawDescriptorSetLayouts[desc._pipelineLayoutBindingIndex]);

				auto* compiled = cache.CompileDescriptorSetLayout(*desc._signature, stageFlags);
				if (compiled) {
					rawDescriptorSetLayouts[desc._pipelineLayoutBindingIndex] = compiled->_layout.get();
					descriptorSetCount = std::max(descriptorSetCount, desc._pipelineLayoutBindingIndex+1);
				}
			}

			rawPushConstantRanges.reserve(rawPushConstantRanges.size() + partial._pushConstants.size());
			for (const auto& s:partial._pushConstants)
				rawPushConstantRanges.push_back(VkPushConstantRange{s._stages, s._rangeStart, s._rangeSize});
		}

		// todo -- we should check if there's any overlaps between push constant ranges
		// todo -- we also need a way to return the blank bindings back to the caller
		return factory.CreatePipelineLayout(
			MakeIteratorRange(rawDescriptorSetLayouts, &rawDescriptorSetLayouts[descriptorSetCount]),
			MakeIteratorRange(rawPushConstantRanges));
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	#if defined(_DEBUG)
		class DescSetLimits
		{
		public:
			unsigned _sampledImageCount;
			unsigned _samplerCount;
			unsigned _uniformBufferCount;
			unsigned _storageBufferCount;
			unsigned _storageImageCount;
			unsigned _inputAttachmentCount;

			void Add(const DescSetLimits& other)
			{
				_sampledImageCount += other._sampledImageCount;
				_samplerCount += other._samplerCount;
				_uniformBufferCount += other._uniformBufferCount;
				_storageBufferCount += other._storageBufferCount;
				_storageImageCount += other._storageImageCount;
				_inputAttachmentCount += other._inputAttachmentCount;
			}
		};

		static DescSetLimits BuildLimits(const DescriptorSetSignature& setSig)
		{
			DescSetLimits result = {};
			for (auto& b:setSig._bindings) {
				switch (b) {
				case DescriptorType::Sampler:
					++result._samplerCount;
					break;

				case DescriptorType::Texture:
					++result._sampledImageCount;
					break;

				case DescriptorType::ConstantBuffer:
					++result._uniformBufferCount;
					break;

				case DescriptorType::UnorderedAccessBuffer:
					++result._storageBufferCount;
					break;

				case DescriptorType::UnorderedAccessTexture:
					++result._storageImageCount;
					break;

				default:
					break;
				}
			}
			return result;
		}

		static void ValidateRootSignature(
			VkPhysicalDevice physDev,
			const DescriptorSetSignatureFile& signatureFile)
		{
			// Validate the root signature against the physical device, and throw an exception
			// if there are problems.
			// Things to check:
			//      VkPhysicalDeviceLimits.maxBoundDescriptorSets
			//      VkPhysicalDeviceLimits.maxPerStageDescriptor*
			//      VkPhysicalDeviceLimits.maxDescriptorSet*

			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(physDev, &props);
			const auto& limits = props.limits;

			for (const auto& rootSig:signatureFile._rootSignatures) {
				if (rootSig._descriptorSets.size() > limits.maxBoundDescriptorSets)
					Throw(::Exceptions::BasicLabel("Root signature exceeds the maximum number of bound descriptor sets supported by device"));
			}

			// Here, we are assuming all descriptors apply equally to all stages.
			DescSetLimits totalLimits = {};
			for (const auto& s:signatureFile._descriptorSets) {
				auto ds = BuildLimits(*s);
				// not really clear how these ones work...?
				if (    ds._sampledImageCount > limits.maxDescriptorSetSampledImages
					||  ds._samplerCount > limits.maxPerStageDescriptorSamplers
					||  ds._uniformBufferCount > limits.maxPerStageDescriptorUniformBuffers
					||  ds._storageBufferCount > limits.maxPerStageDescriptorStorageBuffers
					||  ds._storageImageCount > limits.maxPerStageDescriptorStorageImages
					||  ds._inputAttachmentCount > limits.maxPerStageDescriptorInputAttachments)
					Throw(::Exceptions::BasicLabel("Root signature exceeds the maximum number of bound resources in a single descriptor set that is supported by the device"));
				totalLimits.Add(ds);
			}

			if (    totalLimits._sampledImageCount > limits.maxDescriptorSetSampledImages
				||  totalLimits._samplerCount > limits.maxPerStageDescriptorSamplers
				||  totalLimits._uniformBufferCount > limits.maxPerStageDescriptorUniformBuffers
				||  totalLimits._storageBufferCount > limits.maxPerStageDescriptorStorageBuffers
				||  totalLimits._storageImageCount > limits.maxPerStageDescriptorStorageImages
				||  totalLimits._inputAttachmentCount > limits.maxPerStageDescriptorInputAttachments)
				Throw(::Exceptions::BasicLabel("Root signature exceeds the maximum number of bound resources per stage that is supported by the device"));
		}
	#endif

	VulkanGlobalsTemp& VulkanGlobalsTemp::GetInstance()
	{
		static VulkanGlobalsTemp s_instance;
		return s_instance;
	}

	VkPipelineLayout VulkanGlobalsTemp::GetPipelineLayout(const ShaderProgram&)
	{
		return _graphicsPipelineLayout.get();
	}
	VkPipelineLayout VulkanGlobalsTemp::GetPipelineLayout(const ComputeShader&)
	{
		return _computePipelineLayout.get();
	}

	VulkanGlobalsTemp::VulkanGlobalsTemp() {}
	VulkanGlobalsTemp::~VulkanGlobalsTemp() {}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	std::shared_ptr<PartialPipelineDescriptorsLayout> CreatePartialPipelineDescriptorsLayout(
		const DescriptorSetSignatureFile& signatureFile, PipelineType pipelineType)
	{
		auto result = std::make_shared<PartialPipelineDescriptorsLayout>();

		auto& globals = VulkanGlobalsTemp::GetInstance();
		const auto& root = *signatureFile.GetRootSignature(Hash64(signatureFile._mainRootSignature));
		result->_descriptorSets.reserve(root._descriptorSets.size());
		for (unsigned c=0; c<root._descriptorSets.size(); ++c) {
			const auto&d = root._descriptorSets[c];
			if (d._type == RootSignature::DescriptorSetType::Numeric)
				continue;
			result->_descriptorSets.emplace_back(
				PartialPipelineDescriptorsLayout::DescriptorSet {
					signatureFile.GetDescriptorSet(d._hashName),
					c,
					(unsigned)d._type,
					d._uniformStream,
					d._name
				});
		}

		auto shaderStageMask = 0;
		if (pipelineType == PipelineType::Graphics) {
			shaderStageMask = VK_SHADER_STAGE_ALL_GRAPHICS;
		} else {
			shaderStageMask = VK_SHADER_STAGE_COMPUTE_BIT;
		}

		for (unsigned c=0; c<root._pushConstants.size(); ++c) {
			auto* sig = signatureFile.GetPushConstantsRangeSignature(Hash64(root._pushConstants[c]));
			if (sig && (sig->_stages & shaderStageMask)) {
				auto newSig = *sig;
				newSig._stages &= shaderStageMask;
				result->_pushConstants.push_back(std::move(newSig));
			}
		}

		result->_legacyRegisterBinding = signatureFile.GetLegacyRegisterBinding(Hash64(root._legacyBindings));
		return result;
	}
	
}}}

