// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PipelineLayout.h"
#include "DescriptorSet.h"
#include "ObjectFactory.h"
#include "Pools.h"
#include "IncludeVulkan.h"
#include "../../../Assets/DepVal.h"
#include "../../../Assets/IntermediateAssets.h"		// (for GetDependentFileState)
#include "../../../Assets/IFileSystem.h"
#include "../../../Utility/Streams/StreamDOM.h"
#include "../../../Utility/Streams/StreamFormatter.h"
#include "../../../Utility/Streams/FileUtils.h"
#include "../../../Utility/Threading/Mutex.h"
#include "../../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{

    VkDescriptorType AsVkDescriptorType(DescriptorType type)
    {
        switch (type) {
        case DescriptorType::Sampler:					return VK_DESCRIPTOR_TYPE_SAMPLER;
        case DescriptorType::Texture:					return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorType::ConstantBuffer:			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::UnorderedAccessTexture:	return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case DescriptorType::UnorderedAccessBuffer:		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        default:										return VK_DESCRIPTOR_TYPE_SAMPLER;
        }
    }

    VulkanUniquePtr<VkDescriptorSetLayout> CreateDescriptorSetLayout(
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

	class BoundSignatureFile::Pimpl
    {
    public:
        std::vector<std::pair<uint64_t, BoundPipelineLayout::DescriptorSet>> _descriptorSetLayouts;
		// std::vector<std::pair<uint64_t, std::shared_ptr<BoundPipelineLayout>>> _pipelineLayouts;

        VkShaderStageFlags		_stageFlags;
    };

	const BoundPipelineLayout::DescriptorSet*			BoundSignatureFile::GetDescriptorSetLayout(uint64_t hashName) const
	{
		auto i = LowerBound(_pimpl->_descriptorSetLayouts, hashName);
		if (i != _pimpl->_descriptorSetLayouts.end() && i->first == hashName)
			return &i->second;
		return nullptr;
	}

	/*const std::shared_ptr<BoundPipelineLayout>&		BoundSignatureFile::GetPipelineLayout(uint64_t hashName) const
	{
		auto i = LowerBound(_pimpl->_pipelineLayouts, hashName);
		if (i != _pimpl->_pipelineLayouts.end() && i->first == hashName)
			return i->second;

		static std::shared_ptr<BoundPipelineLayout> s_dummy;
		return s_dummy;
	}*/

	BoundSignatureFile::BoundSignatureFile(
        ObjectFactory& objectFactory,
		GlobalPools& globalPools,
        const DescriptorSetSignatureFile& signatureFile,
        VkShaderStageFlags stageFlags)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_stageFlags = stageFlags;

        // Each descriptor set layout is initialized from the root signature
        // This allows us to create a single global setting that can be used broadly across
        // many "pipelines"

        #if defined(_DEBUG)
            ValidateRootSignature(objectFactory.GetPhysicalDevice(), signatureFile);
        #endif

        _pimpl->_descriptorSetLayouts.reserve(signatureFile._descriptorSets.size());

        for (const auto& s:signatureFile._descriptorSets) {
			BoundPipelineLayout::DescriptorSet ds;
			ds._layout = CreateDescriptorSetLayout(objectFactory, *s, _pimpl->_stageFlags);
			ds._bindings = s->_bindings;
			
			{
				DescriptorSetBuilder builder(globalPools);
				builder.BindDummyDescriptors(*s, uint64_t(s->_bindings.size())-1);
				auto*l = ds._layout.get();
				VulkanUniquePtr<VkDescriptorSet> newDescSet;
				globalPools._mainDescriptorPool.Allocate(
					MakeIteratorRange(&newDescSet, &newDescSet+1),
					MakeIteratorRange(&l, &l+1));
				ds._blankBindings = std::move(newDescSet);
				builder.FlushChanges(
					objectFactory.GetDevice().get(),
					ds._blankBindings.get(),
					0, 0 VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, ds._blankBindingsDescription));
			}

			VULKAN_VERBOSE_DESCRIPTIONS_ONLY(ds._name = s->_name);
			_pimpl->_descriptorSetLayouts.emplace_back(std::make_pair(s->_hashName, std::move(ds)));
        }
		std::sort(
			_pimpl->_descriptorSetLayouts.begin(),
			_pimpl->_descriptorSetLayouts.end(),
			CompareFirst<uint64_t, BoundPipelineLayout::DescriptorSet>());

#if 0
		for (const auto& rootSig:signatureFile._rootSignatures) {
			std::vector<VkDescriptorSetLayout> rawDescriptorSetLayouts;
			rawDescriptorSetLayouts.reserve(rootSig._descriptorSets.size());

			auto boundPipelineLayout = std::make_shared<BoundPipelineLayout>();
			boundPipelineLayout->_descriptorSets.reserve(rootSig._descriptorSets.size());

			for (auto& desc:rootSig._descriptorSets) {
				auto i = LowerBound(_pimpl->_descriptorSetLayouts, desc._hashName);
				if (i == _pimpl->_descriptorSetLayouts.end() || i->first != desc._hashName)
					Throw(::Exceptions::BasicLabel("Could not construct BoundSignatureFile for root signature (%s) because descriptor set (%s) is missing", rootSig._name.c_str(), desc._name.c_str()));

				rawDescriptorSetLayouts.push_back(i->second._layout.get());
				boundPipelineLayout->_descriptorSets.push_back(i->second);
			}

			std::vector<VkPushConstantRange> rawPushConstantRanges;
			rawPushConstantRanges.reserve(rootSig._pushConstants.size());
			for (const auto& s:rootSig._pushConstants) {
				auto i = std::find_if(
					signatureFile._pushConstantRanges.begin(), signatureFile._pushConstantRanges.end(),
					[&s](const PushConstantsRangeSigniture&sig) { return sig._name == s; });
				if (i == signatureFile._pushConstantRanges.end())
					Throw(::Exceptions::BasicLabel("Could not construct BoundSignatureFile for root signature (%s) because push constant range (%s) is missing", rootSig._name.c_str(), s.c_str()));

				// Only need to care if there's an overlap with the stage flags we're building for
				if (i->_stages & stageFlags)
					rawPushConstantRanges.push_back(VkPushConstantRange{i->_stages, i->_rangeStart, i->_rangeSize});
			}

			boundPipelineLayout->_pipelineLayout = objectFactory.CreatePipelineLayout(
				MakeIteratorRange(rawDescriptorSetLayouts),
				MakeIteratorRange(rawPushConstantRanges));

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				boundPipelineLayout->_name = rootSig._name;
			#endif

			_pimpl->_pipelineLayouts.emplace_back(std::make_pair(rootSig._hashName, std::move(boundPipelineLayout)));
		}
		std::sort(
			_pimpl->_pipelineLayouts.begin(),
			_pimpl->_pipelineLayouts.end(),
			CompareFirst<uint64_t, std::shared_ptr<BoundPipelineLayout>>());
#endif
    }

    BoundSignatureFile::~BoundSignatureFile()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

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

	RootSignature::DescriptorSetType AsDescriptorSetType(StringSection<> type)
	{
		for (unsigned c=0; c<dimof(s_descriptorSetTypeNames); ++c)
			if (XlEqString(type, s_descriptorSetTypeNames[c]))
				return (RootSignature::DescriptorSetType)c;
		return RootSignature::DescriptorSetType::Unknown;
	}

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

    static std::shared_ptr<DescriptorSetSignature> ReadDescriptorSet(DocElementHelper<InputStreamFormatter<char>>& element)
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
        auto result = std::make_shared<DescriptorSetSignature>();
		result->_name = element.Attribute("name").Value().AsString();
		result->_hashName = Hash64(result->_name);

		for (auto e=element.FirstChild(); e; e=e.NextSibling()) {
			if (!XlEqString(e.Name(), "Descriptors"))
				Throw(::Exceptions::BasicLabel("Unexpected element while reading DescriptorSetSignature (%s)", result->_name.c_str()));

			auto type = AsDescriptorType(e.Attribute("type").Value());
			auto slots = AsRegisterRange(e.Attribute("slots").Value());

			if (type == DescriptorType::Unknown)
				Throw(::Exceptions::BasicLabel("Descriptor type unrecognized (%s), while reading DescriptorSetSignature (%s)", e.Attribute("type").Value().AsString().c_str(), result->_name.c_str()));

			if (slots._end <= slots._begin)
				Throw(::Exceptions::BasicLabel("Slots attribute not property specified for descriptors in DescriptorSetSignature (%s)", result->_name.c_str()));

            // Add bindings between the start and end (exclusive of end)
			if (result->_bindings.size() < slots._end)
				result->_bindings.resize(slots._end, DescriptorType::Unknown);
            for (auto i=slots._begin; i<slots._end; ++i) {
				if (result->_bindings[i] != DescriptorType::Unknown)
					Throw(::Exceptions::BasicLabel("Some descriptor slots overlap while reading DescriptorSetSignature (%s)", result->_name.c_str()));
                result->_bindings[i] = type;
			}
        }

		for (const auto&t:result->_bindings)
			if (t == DescriptorType::Unknown)
				Throw(::Exceptions::BasicLabel("Gap between descriptor slots while reading DescriptorSetSignature (%s)", result->_name.c_str()));

        return result;
    }

	static std::shared_ptr<LegacyRegisterBinding> ReadLegacyRegisterBinding(
		DocElementHelper<InputStreamFormatter<char>>& element,
		IteratorRange<const StringSection<>*> descriptorSetNames)
	{
		auto result = std::make_shared<LegacyRegisterBinding>();
		result->_name = element.Attribute("name").Value().AsString();
		result->_hashName = Hash64(result->_name);

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
			case LegacyRegisterBinding::RegisterType::Sampler: dest = &result->_samplerRegisters; break;
			case LegacyRegisterBinding::RegisterType::ShaderResource:
				dest = (legacyRegisters._qualifier == LegacyRegisterBinding::RegisterQualifier::Buffer) ? &result->_srvRegisters_boundToBuffer : &result->_srvRegisters;
				break;
			case LegacyRegisterBinding::RegisterType::ConstantBuffer: dest = &result->_constantBufferRegisters; break;
			case LegacyRegisterBinding::RegisterType::UnorderedAccess:
				dest = (legacyRegisters._qualifier == LegacyRegisterBinding::RegisterQualifier::Buffer) ? &result->_uavRegisters_boundToBuffer : &result->_uavRegisters; 
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
        PushConstantsRangeSigniture result;
        for (auto a=element.FirstAttribute(); a; a=a.Next()) {
            if (a.Name().IsEmpty()) continue;

            if (XlEqStringI(a.Name(), "name")) {
                result._name = a.Value().AsString();
				result._hashName = Hash64(result._name);
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

	static RootSignature ReadRootSignature(DocElementHelper<InputStreamFormatter<char>>& element)
    {
		RootSignature result;
		result._name = element.Attribute("name").Value().AsString();
		result._hashName = Hash64(result._name);
		result._legacyBindings = element.Attribute("legacyBindings").Value().AsString();

		for (auto e=element.FirstChild(); e; e=e.NextSibling()) {
			if (XlEqString(e.Name(), "Set")) {
				RootSignature::DescriptorSetReference ref;
				ref._type = AsDescriptorSetType(e.Attribute("type").Value());
				ref._uniformStream = e.Attribute("uniformStream", ~0u);
				ref._name = e.Attribute("name").Value().AsString();
				ref._hashName = Hash64(ref._name);
				result._descriptorSets.emplace_back(std::move(ref));
			} else if (XlEqString(e.Name(), "PushConstants")) {
				result._pushConstants.emplace_back(e.Attribute("name").Value().AsString());
			} else {
				Throw(::Exceptions::BasicLabel("Unexpected element (%s) while reading root signature (%s)", e.Name().AsString().c_str(), result._name.c_str()));
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

	const RootSignature*								DescriptorSetSignatureFile::GetRootSignature(uint64_t name) const
	{
		for (const auto&r:_rootSignatures)
			if (r._hashName == name)
				return &r;
		return nullptr;
	}

	const std::shared_ptr<LegacyRegisterBinding>&		DescriptorSetSignatureFile::GetLegacyRegisterBinding(uint64_t name) const
	{
		static std::shared_ptr<LegacyRegisterBinding> dummy;
		// auto* rootSig = GetRootSignature(name);
		// if (!rootSig) return dummy;

		// auto h = Hash64(rootSig->_legacyBindings);
		for (const auto&r:_legacyRegisterBindingSettings)
			if (r->_hashName == name)
				return r;
		return dummy;
	}

	const PushConstantsRangeSigniture*					DescriptorSetSignatureFile::GetPushConstantsRangeSigniture(uint64_t name) const
	{
		// auto* rootSig = GetRootSignature(name);
		// if (!rootSig) return nullptr;

		// auto h = Hash64(rootSig->_pushConstants[0]);
		for (const auto&r:_pushConstantRanges)
			if (r._hashName == name)
				return &r;
		return nullptr;
	}

	const std::shared_ptr<DescriptorSetSignature>&		DescriptorSetSignatureFile::GetDescriptorSet(uint64_t name) const
	{
		static std::shared_ptr<DescriptorSetSignature> dummy;
		for (const auto&d:_descriptorSets)
			if (d->_hashName == name)
				return d;
		return dummy;
	}

    DescriptorSetSignatureFile::DescriptorSetSignatureFile(StringSection<> filename)
    {
		_depVal = std::make_shared<::Assets::DependencyValidation>();
		::Assets::RegisterFileDependency(_depVal, filename);

		TRY {
			// attempt to load the source file and extract the root signature
			size_t fileSize = 0;
			auto block = ::Assets::TryLoadFileAsMemoryBlock(filename, &fileSize);
			if (!block || !fileSize)
				Throw(::Exceptions::BasicLabel("Failure while attempting to load descriptor set signature file (%s)", filename.AsString().c_str()));

			_dependentFileState = Assets::IntermediateAssets::Store::GetDependentFileState(filename);
        

			InputStreamFormatter<char> formatter(
				MemoryMappedInputStream(block.get(), PtrAdd(block.get(), fileSize)));
			Document<InputStreamFormatter<char>> doc(formatter);

			_mainRootSignature = doc.Attribute("MainRootSignature").Value().AsString();
			if (_mainRootSignature.empty())
				Throw(::Exceptions::BasicLabel("Main root root signature not specified while loading file (%s)", filename.AsString().c_str()));

			for (auto a=doc.FirstChild(); a; a=a.NextSibling()) {
				if (XlEqString(a.Name(), "DescriptorSet")) {
					_descriptorSets.emplace_back(ReadDescriptorSet(a));
				} else if (XlEqString(a.Name(), "LegacyBinding")) {
					std::vector<StringSection<>> descriptorSetNames;
					descriptorSetNames.reserve(_descriptorSets.size());
					for (const auto&d:_descriptorSets) descriptorSetNames.push_back(d->_name);
					_legacyRegisterBindingSettings.emplace_back(ReadLegacyRegisterBinding(a, MakeIteratorRange(descriptorSetNames)));
				} else if (XlEqString(a.Name(), "PushConstants")) {
					_pushConstantRanges.emplace_back(ReadPushConstRange(a));
				} else if (XlEqString(a.Name(), "RootSignature")) {
					_rootSignatures.emplace_back(ReadRootSignature(a));
				} else {
					Throw(::Exceptions::BasicLabel("Unexpected element type (%s) while loading descriptor set signature file", a.Name().AsString().c_str(), filename.AsString().c_str()));
				}
			}
		} CATCH(const ::Assets::Exceptions::ConstructionError& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, _depVal));
		} CATCH(const std::exception& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, _depVal));
		} CATCH_END
    }

    DescriptorSetSignatureFile::~DescriptorSetSignatureFile() {}


	const std::shared_ptr<BoundSignatureFile>& VulkanGlobalsTemp::GetBoundPipelineLayout(ObjectFactory& objectFactory, const DescriptorSetSignatureFile& input, VkShaderStageFlags stageFlags)
	{
		auto i = _boundFiles.find(&input);
		if (i != _boundFiles.end())
			return i->second;

		auto n = std::make_shared<BoundSignatureFile>(objectFactory, *_globalPools, input, stageFlags);
		i = _boundFiles.insert(std::make_pair(&input, n)).first;
		return i->second;
	}

	VulkanGlobalsTemp& VulkanGlobalsTemp::GetInstance()
	{
		static VulkanGlobalsTemp s_instance;
		return s_instance;
	}

	VulkanGlobalsTemp::VulkanGlobalsTemp() {}
	VulkanGlobalsTemp::~VulkanGlobalsTemp() {}
}}

