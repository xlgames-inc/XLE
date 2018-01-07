// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PipelineLayout.h"
#include "ObjectFactory.h"
#include "IncludeVulkan.h"
#include "../../../Assets/IntermediateAssets.h"
#include "../../../Assets/IFileSystem.h"
#include "../../../Utility/Streams/StreamDOM.h"
#include "../../../Utility/Streams/StreamFormatter.h"
#include "../../../Utility/Streams/FileUtils.h"
#include "../../../Utility/Threading/Mutex.h"

namespace RenderCore { namespace Metal_Vulkan
{

    class PipelineLayout::Pimpl
    {
    public:
        std::vector<VulkanUniquePtr<VkDescriptorSetLayout>> _descriptorSetLayout;
        VulkanUniquePtr<VkPipelineLayout>                   _pipelineLayout;

        std::shared_ptr<RootSignature>      _rootSignature;
        ::Assets::rstring                   _rootSignatureFilename;
        Threading::Mutex                    _rootSignatureLock;

        VkShaderStageFlags  _stageFlags;

        bool _pendingLayoutRebuild;
    };

    VkDescriptorSetLayout       PipelineLayout::GetDescriptorSetLayout(unsigned index)
    {
        assert(index < (unsigned)_pimpl->_descriptorSetLayout.size());
        return _pimpl->_descriptorSetLayout[index].get();
    }

    unsigned                    PipelineLayout::GetDescriptorSetCount()
    {
        return (unsigned)_pimpl->_descriptorSetLayout.size();
    }

    VkPipelineLayout            PipelineLayout::GetUnderlying()
    {
        return _pimpl->_pipelineLayout.get();
    }

    VkDescriptorType AsDescriptorType(DescriptorSetBindingSignature::Type type)
    {
        switch (type) {
        case DescriptorSetBindingSignature::Type::Sampler:                  return VK_DESCRIPTOR_TYPE_SAMPLER;
        case DescriptorSetBindingSignature::Type::Texture:                 return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorSetBindingSignature::Type::ConstantBuffer:           return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorSetBindingSignature::Type::UnorderedAccess:          return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        case DescriptorSetBindingSignature::Type::TextureAsBuffer:         return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorSetBindingSignature::Type::UnorderedAccessAsBuffer:  return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        default:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        }
    }

    static VulkanUniquePtr<VkDescriptorSetLayout> CreateDescriptorSetLayout(
        const ObjectFactory& factory, 
        const DescriptorSetSignature& srcLayout,
        VkShaderStageFlags stageFlags)
    {
        // The "root signature" bindings correspond very closely with the
        // DescriptorSetLayout
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(srcLayout._bindings.size());
        for (unsigned bIndex=0; bIndex<(unsigned)srcLayout._bindings.size(); ++bIndex) {
            VkDescriptorSetLayoutBinding dstBinding = {};
            dstBinding.binding = bIndex;
            dstBinding.descriptorType = AsDescriptorType(srcLayout._bindings[bIndex]._type);
            dstBinding.descriptorCount = 1;
            dstBinding.stageFlags = stageFlags;
            dstBinding.pImmutableSamplers = nullptr;
            bindings.push_back(dstBinding);
        }
        return factory.CreateDescriptorSetLayout(MakeIteratorRange(bindings));
    }

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
                switch (b._type) {
                case DescriptorSetBindingSignature::Type::Sampler:
                    ++result._samplerCount;
                    break;

                case DescriptorSetBindingSignature::Type::Texture:
                    ++result._sampledImageCount;
                    break;

                case DescriptorSetBindingSignature::Type::ConstantBuffer:
                    ++result._uniformBufferCount;
                    break;

                case DescriptorSetBindingSignature::Type::TextureAsBuffer:
                case DescriptorSetBindingSignature::Type::UnorderedAccessAsBuffer:
                    ++result._storageBufferCount;
                    break;

                case DescriptorSetBindingSignature::Type::UnorderedAccess:
                    ++result._storageImageCount;
                    break;
                }
            }
            return result;
        }

        static void ValidateRootSignature(VkPhysicalDevice physDev, const RootSignature& sig)
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

            if (sig._descriptorSets.size() > limits.maxBoundDescriptorSets)
                Throw(::Exceptions::BasicLabel("Root signature exceeds the maximum number of bound descriptor sets supported by device"));

            // Here, we are assuming all descriptors apply equally to all stages.
            DescSetLimits totalLimits = {};
            for (const auto& s:sig._descriptorSets) {
                auto ds = BuildLimits(s);
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

    void PipelineLayout::RebuildLayout(const ObjectFactory& factory)
    {
        // Rebuild the pipeline layout, but only if something has changed
        if (_pimpl->_pipelineLayout && !_pimpl->_pendingLayoutRebuild)
            return;

        // Each descriptor set layout is initialized from the root signature
        // This allows us to create a single global setting that can be used broadly across
        // many "pipelines"

        _pimpl->_descriptorSetLayout.clear();
        _pimpl->_pipelineLayout.reset();
        auto rootSig = ShareRootSignature();

        #if defined(_DEBUG)
            ValidateRootSignature(factory.GetPhysicalDevice(), *rootSig);
        #endif

        std::vector<VkDescriptorSetLayout> rawDescriptorSetLayouts;
        _pimpl->_descriptorSetLayout.reserve(rootSig->_descriptorSets.size());
        rawDescriptorSetLayouts.reserve(rootSig->_descriptorSets.size());

        for (const auto& s:rootSig->_descriptorSets) {
            auto layout = CreateDescriptorSetLayout(factory, s, _pimpl->_stageFlags);
            rawDescriptorSetLayouts.push_back(layout.get());
            _pimpl->_descriptorSetLayout.emplace_back(std::move(layout));
        }

        std::vector<VkPushConstantRange> rawPushConstantRanges;
        rawPushConstantRanges.reserve(rootSig->_pushConstantRanges.size());
        for (const auto& r:rootSig->_pushConstantRanges)
            rawPushConstantRanges.push_back(VkPushConstantRange{r._stages, r._rangeStart, r._rangeSize});

        _pimpl->_pipelineLayout = factory.CreatePipelineLayout(
            MakeIteratorRange(rawDescriptorSetLayouts),
            MakeIteratorRange(rawPushConstantRanges));
        _pimpl->_pendingLayoutRebuild = false;
    }

    std::shared_ptr<RootSignature> PipelineLayout::ShareRootSignature()
    {
        // this method can be called simulateously from multiple threads
        ScopedLock(_pimpl->_rootSignatureLock);
        if (!_pimpl->_rootSignature || _pimpl->_rootSignature->GetDependencyValidation()->GetValidationIndex() != 0) {
            _pimpl->_rootSignature = std::make_shared<RootSignature>(_pimpl->_rootSignatureFilename.c_str());
            _pimpl->_pendingLayoutRebuild = true;
        }
        return _pimpl->_rootSignature;
    }

    PipelineLayout::PipelineLayout(
        const ObjectFactory& objectFactory,
        StringSection<::Assets::ResChar> rootSignatureCfg,
        VkShaderStageFlags stageFlags)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_pendingLayoutRebuild = true;
        _pimpl->_rootSignatureFilename = rootSignatureCfg.AsString();
        _pimpl->_stageFlags = stageFlags;
        RebuildLayout(objectFactory);
    }

    PipelineLayout::~PipelineLayout()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    enum class Qualifier { None, Image, Buffer };

    static Qualifier AsQualifier(StringSection<char> str)
    {
        // look for "(image)" or "(buffer)" qualifiers
        if (str.IsEmpty() || str[0] != '(') return Qualifier::None;

        if (XlEqStringI(StringSection<char>(str.begin()+1, str.end()), "buffer)"))
            return Qualifier::Buffer;

        if (XlEqStringI(StringSection<char>(str.begin()+1, str.end()), "image)"))
            return Qualifier::Image;

        return Qualifier::None;
    }

    static unsigned AsShaderStageMask(StringSection<char> str)
    {
        if (str.IsEmpty() || str[0] != '(')
            return VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        unsigned result = 0u;
        auto* i = &str[1];
        while (i != str.end() && *i != ')') {
            switch (*i) {
            case 'v': result |= VK_SHADER_STAGE_VERTEX_BIT;
            case 'f': result |= VK_SHADER_STAGE_FRAGMENT_BIT;
            case 'g': result |= VK_SHADER_STAGE_GEOMETRY_BIT;
            case 'c': result |= VK_SHADER_STAGE_COMPUTE_BIT;
            case 'd': result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            case 'h': result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            }
            ++i;
        }
        return result;
    }

    static DescriptorSetBindingSignature::Type AsBindingType(char type, Qualifier qualifier)
    {
        // convert between HLSL style register binding indices to a type enum
        switch (type) {
        case 'b': return DescriptorSetBindingSignature::Type::ConstantBuffer;
        case 's': return DescriptorSetBindingSignature::Type::Sampler;
        case 't': 
            if (qualifier == Qualifier::Buffer)
                return DescriptorSetBindingSignature::Type::TextureAsBuffer;
            return DescriptorSetBindingSignature::Type::Texture;
        case 'u': 
            if (qualifier == Qualifier::Buffer)
                return DescriptorSetBindingSignature::Type::UnorderedAccessAsBuffer;
            return DescriptorSetBindingSignature::Type::UnorderedAccess;

        default:  return DescriptorSetBindingSignature::Type::Unknown;
        }
    }

    static DescriptorSetSignature ReadDescSet(DocElementHelper<InputStreamFormatter<char>>& element)
    {
        // Create a DescriptorSetLayout from the given document element
        // The element should be a series of attributes of the form
        //      b11..20
	    //      t11..20
        //      u3
        //
        // A single character represents the type. It should be followed by 
        // either a single number or an (inclusive) range.
        // SM5.1 adds a "space" parameter to allow for overlaps. But we don't support this currently.
        DescriptorSetSignature result;
        for (auto a=element.FirstAttribute(); a; a=a.Next()) {
            if (a.Name().IsEmpty()) continue;

            if (XlEqStringI(a.Name(), "Name")) {
                result._name = a.Value().AsString();
                continue;
            }

            char* endPt = nullptr;
            auto start = std::strtoul(&a.Name()[1], &endPt, 10);
            auto end = start+1;
            if (endPt && endPt[0] == '.' && endPt[1] == '.')
                end = std::strtoul(endPt+2, &endPt, 10);

            auto qualifier = AsQualifier(StringSection<char>(endPt, a.Name().end()));
            auto type = AsBindingType(a.Name()[0], qualifier);

            // Add bindings between the start and end (exclusive of end)
            for (auto i=start; i<end; ++i)
                result._bindings.push_back(DescriptorSetBindingSignature{type, i});
        }
        return std::move(result);
    }

    static PushConstantsRangeSigniture ReadPushConstRange(DocElementHelper<InputStreamFormatter<char>>& element)
    {
        PushConstantsRangeSigniture result = {std::string(), 0u, 0u, 0u};
        for (auto a=element.FirstAttribute(); a; a=a.Next()) {
            if (a.Name().IsEmpty()) continue;

            if (XlEqStringI(a.Name(), "Name")) {
                result._name = a.Value().AsString();
                continue;
            }

            char* endPt = nullptr;
            auto start = std::strtoul(a.Name().begin(), &endPt, 10);
            auto end = start;
            if (endPt && endPt[0] == '.' && endPt[1] == '.')
                end = std::strtoul(endPt+2, &endPt, 10);

            result._stages |= AsShaderStageMask(StringSection<char>(endPt, a.Name().end()));
            result._rangeStart = start;
            result._rangeSize = end-start;
        }
        return result;
    }

    RootSignature::RootSignature(StringSection<::Assets::ResChar> filename)
    {
		_depVal = std::make_shared<::Assets::DependencyValidation>();
		::Assets::RegisterFileDependency(_depVal, filename);

		TRY {
			// attempt to load the source file and extract the root signature
			size_t fileSize = 0;
			auto block = ::Assets::TryLoadFileAsMemoryBlock(filename, &fileSize);
			if (!block || !fileSize)
				Throw(::Exceptions::BasicLabel("Failure while attempting to load root signature (%s)", filename));

			_dependentFileState = Assets::IntermediateAssets::Store::GetDependentFileState(filename);
        

			InputStreamFormatter<char> formatter(
				MemoryMappedInputStream(block.get(), PtrAdd(block.get(), fileSize)));
			Document<InputStreamFormatter<char>> doc(formatter);

			std::vector<StringSection<>> rootSig;
			for (auto a=doc.FirstChild(); a; a=a.NextSibling()) {
				auto name = a.Name();
				if (XlEqString(name, "Set")) {
					_descriptorSets.emplace_back(ReadDescSet(a));
				} else if (XlEqString(name, "PushConstants")) {
					_pushConstantRanges.emplace_back(ReadPushConstRange(a));
				}
			}
		} CATCH(const ::Assets::Exceptions::ConstructionError& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, _depVal));
		} CATCH(const std::exception& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, _depVal));
		} CATCH_END
    }

    RootSignature::~RootSignature() {}
}}

