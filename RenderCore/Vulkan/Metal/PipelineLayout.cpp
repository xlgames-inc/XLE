// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PipelineLayout.h"
#include "ObjectFactory.h"
#include "IncludeVulkan.h"
#include "../../../Assets/DepVal.h"
#include "../../../Assets/IntermediateAssets.h"		// (for GetDependentFileState)
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

    VkDescriptorSetLayout PipelineLayout::GetDescriptorSetLayout(unsigned index)
    {
        assert(index < (unsigned)_pimpl->_descriptorSetLayout.size());
        return _pimpl->_descriptorSetLayout[index].get();
    }

	const DescriptorSetSignature&	PipelineLayout::GetDescriptorSetSignature(unsigned index)
	{
		assert(index < (unsigned)_pimpl->_rootSignature->_descriptorSets.size());
		return _pimpl->_rootSignature->_descriptorSets[index];
	}

    unsigned                    PipelineLayout::GetDescriptorSetCount()
    {
        return (unsigned)_pimpl->_descriptorSetLayout.size();
    }

    VkPipelineLayout            PipelineLayout::GetUnderlying()
    {
        return _pimpl->_pipelineLayout.get();
    }

    VkDescriptorType AsVkDescriptorType(DescriptorType type)
    {
        switch (type) {
        case DescriptorType::Sampler:					return VK_DESCRIPTOR_TYPE_SAMPLER;
        case DescriptorType::Texture:					return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorType::ConstantBuffer:			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::UnorderedAccessTexture:	return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case DescriptorType::UnorderedAccessBuffer:		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        default: return VK_DESCRIPTOR_TYPE_SAMPLER;
        }
    }

	static const char* s_descriptorTypeNames[] = {
		"Sampler",
		"Texture",
		"ConstantBuffer",
		"UnorderedAccessTexture",
		"UnorderedAccessBuffer"
	};

	static const char* s_descriptorSetTypeNames[] = {
		"Adaptive", "Numeric", "Unknown"
	};

	const char* AsString(DescriptorType type)
	{
		if (unsigned(type) < dimof(s_descriptorTypeNames))
			return s_descriptorTypeNames[unsigned(type)];
        return "<<unknown>>";
	}

	DescriptorType AsDescriptorType(StringSection<> type)
	{
		for (unsigned c=0; c<dimof(s_descriptorTypeNames); ++c)
			if (XlEqString(type, s_descriptorTypeNames[c]))
				return (DescriptorType)c;
		return DescriptorType::Unknown;
	}

	DescriptorSetSignature::Type AsDescriptorSetType(StringSection<> type)
	{
		for (unsigned c=0; c<dimof(s_descriptorSetTypeNames); ++c)
			if (XlEqString(type, s_descriptorSetTypeNames[c]))
				return (DescriptorSetSignature::Type)c;
		return DescriptorSetSignature::Type::Unknown;
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
            dstBinding.descriptorType = AsVkDescriptorType(srcLayout._bindings[bIndex]);
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
        auto rootSig = GetRootSignature();

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

    const std::shared_ptr<RootSignature>& PipelineLayout::GetRootSignature()
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

    static LegacyRegisterBinding::RegisterQualifier AsQualifier(StringSection<char> str)
    {
        // look for "(image)" or "(buffer)" qualifiers
        if (str.IsEmpty() || str[0] != '(') return LegacyRegisterBinding::RegisterQualifier::None;

        if (XlEqStringI(StringSection<char>(str.begin()+1, str.end()), "buffer)"))
            return LegacyRegisterBinding::RegisterQualifier::Buffer;

        if (XlEqStringI(StringSection<char>(str.begin()+1, str.end()), "texture)"))
            return LegacyRegisterBinding::RegisterQualifier::Texture;

        return LegacyRegisterBinding::RegisterQualifier::None;
    }

	struct RegisterRange
	{
		unsigned _begin = 0, _end = 0;
		LegacyRegisterBinding::RegisterQualifier _qualifier;
	};

	static RegisterRange AsRegisterRange(StringSection<> input)
	{
		if (input.IsEmpty()) return {};

		char* endPt = nullptr;
        auto start = std::strtoul(input.begin(), &endPt, 10);
        auto end = start+1;
        if (endPt && endPt[0] == '.' && endPt[1] == '.')
            end = std::strtoul(endPt+2, &endPt, 10);

        auto qualifier = AsQualifier(StringSection<char>(endPt, input.end()));
		return {start, end, qualifier};
	}

    static unsigned AsShaderStageMask(StringSection<char> str)
    {
        if (str.IsEmpty() || str[0] != '(')
            return VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        unsigned result = 0u;
        auto* i = &str[1];
        while (i != str.end() && *i != ')') {
            switch (*i) {
            case 'v': result |= VK_SHADER_STAGE_VERTEX_BIT; break;
            case 'f': result |= VK_SHADER_STAGE_FRAGMENT_BIT; break;
            case 'g': result |= VK_SHADER_STAGE_GEOMETRY_BIT; break;
            case 'c': result |= VK_SHADER_STAGE_COMPUTE_BIT; break;
            case 'd': result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT; break;
            case 'h': result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT; break;
            }
            ++i;
        }
        return result;
    }

    static LegacyRegisterBinding::RegisterType AsLegacyRegisterType(char type)
    {
        // convert between HLSL style register binding indices to a type enum
        switch (type) {
        case 'b': return LegacyRegisterBinding::RegisterType::ConstantBuffer;
        case 's': return LegacyRegisterBinding::RegisterType::Sampler;
        case 't': return LegacyRegisterBinding::RegisterType::ShaderResource;
        case 'u': return LegacyRegisterBinding::RegisterType::UnorderedAccess;
	    default:  return LegacyRegisterBinding::RegisterType::Unknown;
        }
    }

    static DescriptorSetSignature ReadDescriptorSet(DocElementHelper<InputStreamFormatter<char>>& element)
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

		result._name = element.Attribute("name").Value().AsString();
		result._type = AsDescriptorSetType(element.Attribute("type").Value());
		result._uniformStream = element.Attribute("uniformStream", ~0u);

		if (result._name.empty() || result._type == DescriptorSetSignature::Type::Unknown)
			Throw(::Exceptions::BasicLabel("Error while reading DescriptorSetSignature (%s)", result._name.c_str()));

		for (auto e=element.FirstChild(); e; e=e.NextSibling()) {
			if (!XlEqString(e.Name(), "Descriptors"))
				Throw(::Exceptions::BasicLabel("Unexpected element while reading DescriptorSetSignature (%s)", result._name.c_str()));

			auto type = AsDescriptorType(e.Attribute("type").Value());
			auto slots = AsRegisterRange(e.Attribute("slots").Value());

			if (type == DescriptorType::Unknown)
				Throw(::Exceptions::BasicLabel("Descriptor type unrecognized (%s), while reading DescriptorSetSignature (%s)", e.Attribute("type").Value().AsString().c_str(), result._name.c_str()));

			if (slots._end <= slots._begin)
				Throw(::Exceptions::BasicLabel("Slots attribute not property specified for descriptors in DescriptorSetSignature (%s)", result._name.c_str()));

            // Add bindings between the start and end (exclusive of end)
			if (result._bindings.size() < slots._end)
				result._bindings.resize(slots._end, DescriptorType::Unknown);
            for (auto i=slots._begin; i<slots._end; ++i) {
				if (result._bindings[i] != DescriptorType::Unknown)
					Throw(::Exceptions::BasicLabel("Some descriptor slots overlap while reading DescriptorSetSignature (%s)", result._name.c_str()));
                result._bindings[i] = type;
			}
        }

		for (const auto&t:result._bindings)
			if (t == DescriptorType::Unknown)
				Throw(::Exceptions::BasicLabel("Gap between descriptor slots while reading DescriptorSetSignature (%s)", result._name.c_str()));

        return result;
    }

	static LegacyRegisterBinding ReadLegacyRegisterBinding(
		DocElementHelper<InputStreamFormatter<char>>& element,
		IteratorRange<const StringSection<>*> descriptorSetNames)
	{
		LegacyRegisterBinding result;

		for (auto e=element.FirstChild(); e; e=e.NextSibling()) {
			auto name = e.Name();
			if (name.IsEmpty())
				Throw(std::runtime_error("Legacy register binding with empty name"));

			auto regType = AsLegacyRegisterType(name[0]);
			if (regType == LegacyRegisterBinding::RegisterType::Unknown)
				Throw(::Exceptions::BasicLabel("Could not parse legacy register binding (%s)", name.AsString().c_str()));

			auto legacyRegisters = AsRegisterRange({name.begin()+1, name.end()});
			if (legacyRegisters._end <= legacyRegisters._begin)
				Throw(::Exceptions::BasicLabel("Could not parse legacy register binding (%s)", name.AsString().c_str()));

			auto mappedRegisters = AsRegisterRange(e.Attribute("mapping").Value());
			if (mappedRegisters._begin == mappedRegisters._end)
				Throw(::Exceptions::BasicLabel("Could not parse target register mapping in ReadLegacyRegisterBinding (%s)", e.Attribute("mapping").Value().AsString().c_str()));
			
			if ((mappedRegisters._end - mappedRegisters._begin) != (legacyRegisters._end - legacyRegisters._begin))
				Throw(::Exceptions::BasicLabel("Number of legacy register and number of mapped registers don't match up in ReadLegacyRegisterBinding"));

			auto set = e.Attribute("set").Value();
			auto i = std::find_if(
				descriptorSetNames.begin(), descriptorSetNames.end(),
				[set](StringSection<> compare) { return XlEqString(set, compare); });
			if (i == descriptorSetNames.end())
				Throw(::Exceptions::BasicLabel("Could not find referenced descriptor set in ReadLegacyRegisterBinding (%s)", set.AsString().c_str()));

			std::vector<LegacyRegisterBinding::Entry>* dest =  nullptr;
			switch (regType) {
			case LegacyRegisterBinding::RegisterType::Sampler: dest = &result._samplerRegisters; break;
			case LegacyRegisterBinding::RegisterType::ShaderResource:
				dest = (legacyRegisters._qualifier == LegacyRegisterBinding::RegisterQualifier::Buffer) ? &result._srvRegisters_boundToBuffer : &result._srvRegisters;
				break;
			case LegacyRegisterBinding::RegisterType::ConstantBuffer: dest = &result._constantBufferRegisters; break;
			case LegacyRegisterBinding::RegisterType::UnorderedAccess:
				dest = (legacyRegisters._qualifier == LegacyRegisterBinding::RegisterQualifier::Buffer) ? &result._uavRegisters_boundToBuffer : &result._uavRegisters; 
				break;
			default: assert(0);
			}

			auto di = dest->begin();
			while (di!=dest->end() && di->_begin < legacyRegisters._end) ++di;

			if (di != dest->end() && di->_begin < legacyRegisters._end)
				Throw(::Exceptions::BasicLabel("Register overlap found in ReadLegacyRegisterBinding"));

			dest->insert(di, LegacyRegisterBinding::Entry {
				legacyRegisters._begin, legacyRegisters._end,
				(unsigned)std::distance(descriptorSetNames.begin(), i),
				mappedRegisters._begin, mappedRegisters._end });
		}

		return result;
	}

    static PushConstantsRangeSigniture ReadPushConstRange(DocElementHelper<InputStreamFormatter<char>>& element)
    {
        PushConstantsRangeSigniture result = {std::string(), 0u, 0u, 0u};
        for (auto a=element.FirstAttribute(); a; a=a.Next()) {
            if (a.Name().IsEmpty()) continue;

            if (XlEqStringI(a.Name(), "name")) {
                result._name = a.Value().AsString();
                continue;
            }

            char* endPt = nullptr;
            auto start = std::strtoul(a.Name().begin(), &endPt, 10);
            auto end = start;
            if (endPt && endPt[0] == '.' && endPt[1] == '.')
                end = std::strtoul(endPt+2, &endPt, 10);

			if (start != end) {
				result._stages |= AsShaderStageMask(StringSection<char>(endPt, a.Name().end()));
				result._rangeStart = start;
				result._rangeSize = end-start;
			}
        }
        return result;
    }

	IteratorRange<const LegacyRegisterBinding::Entry*>	LegacyRegisterBinding::GetEntries(RegisterType type, RegisterQualifier qualifier) const
	{
		switch (type) {
		case RegisterType::Sampler: return MakeIteratorRange(_samplerRegisters);
		case RegisterType::ShaderResource: return (qualifier == RegisterQualifier::Buffer) ? MakeIteratorRange(_srvRegisters_boundToBuffer) : MakeIteratorRange(_srvRegisters);
		case RegisterType::ConstantBuffer: return MakeIteratorRange(_constantBufferRegisters);
		case RegisterType::UnorderedAccess: return (qualifier == RegisterQualifier::Buffer) ? MakeIteratorRange(_uavRegisters_boundToBuffer) : MakeIteratorRange(_uavRegisters);
		}

		return {};
	}

	char GetRegisterPrefix(LegacyRegisterBinding::RegisterType regType)
	{
		switch (regType) {
		case LegacyRegisterBinding::RegisterType::Sampler: return 's';
		case LegacyRegisterBinding::RegisterType::ShaderResource: return 't';
		case LegacyRegisterBinding::RegisterType::ConstantBuffer: return 'b';
		case LegacyRegisterBinding::RegisterType::UnorderedAccess: return 'u';
		}
		return ' ';
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
				Throw(::Exceptions::BasicLabel("Failure while attempting to load root signature (%s)", filename.AsString().c_str()));

			_dependentFileState = Assets::IntermediateAssets::Store::GetDependentFileState(filename);
        

			InputStreamFormatter<char> formatter(
				MemoryMappedInputStream(block.get(), PtrAdd(block.get(), fileSize)));
			Document<InputStreamFormatter<char>> doc(formatter);

			auto mainRootSignature = doc.Attribute("MainRootSignature").Value();
			if (mainRootSignature.IsEmpty())
				Throw(::Exceptions::BasicLabel("Main root root signature not specified while loading file (%s)", filename.AsString().c_str()));

			std::vector<DescriptorSetSignature> descriptorSets;

			for (auto a=doc.FirstChild(); a; a=a.NextSibling()) {
				if (XlEqString(a.Name(), "DescriptorSet")) {
					descriptorSets.emplace_back(ReadDescriptorSet(a));
				} else if (XlEqString(a.Name(), "RootSignature") && XlEqString(a.Attribute("name").Value(), mainRootSignature)) {
					
					for (auto b=a.FirstAttribute(); b; b=b.Next()) {
						if (XlEqString(b.Name(), "set")) {
							auto i = std::find_if(
								descriptorSets.begin(), descriptorSets.end(),
								[b](const DescriptorSetSignature& d) { return XlEqString(b.Value(), d._name); });
							if (i == descriptorSets.end())
								Throw(::Exceptions::BasicLabel("Could not find descriptor set with name (%s) loading file (%s)", b.Value().AsString().c_str(), filename.AsString().c_str()));
							_descriptorSets.push_back(*i);
						}
					}

					for (auto b=a.FirstChild(); b; b=b.NextSibling()) {
						if (XlEqString(b.Name(), "PushConstants")) {
							_pushConstantRanges.emplace_back(ReadPushConstRange(b));
						} else if (XlEqString(b.Name(), "LegacyBinding")) {
							std::vector<StringSection<>> descriptorSetNames;
							descriptorSetNames.reserve(_descriptorSets.size());
							for (const auto&d:_descriptorSets) descriptorSetNames.push_back(d._name);
							_legacyBinding = ReadLegacyRegisterBinding(b, MakeIteratorRange(descriptorSetNames));
						}
					}
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

