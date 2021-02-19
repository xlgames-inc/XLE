// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if 0 

#include "DescriptorSetSignatureFile.h"
#include "../../UniformsStream.h"
#include "../../../Assets/DepVal.h"
#include "../../../Assets/IntermediatesStore.h"		// (for GetDependentFileState)
#include "../../../Assets/IFileSystem.h"
#include "../../../Utility/MemoryUtils.h"
#include "../../../Utility/Streams/StreamDOM.h"
#include "../../../Utility/Streams/StreamFormatter.h"
#include "../../../OSServices/RawFS.h"
#include <stdexcept>

#include "IncludeVulkan.h"		// for VK_SHADER_STAGE_ stuff below

namespace RenderCore { namespace Metal_Vulkan
{
	static const char* s_descriptorSetTypeNames[] = {
		"Adaptive", "Numeric", "Unknown"
	};

	RootSignature::DescriptorSetType AsDescriptorSetType(StringSection<> type)
	{
		for (unsigned c=0; c<dimof(s_descriptorSetTypeNames); ++c)
			if (XlEqString(type, s_descriptorSetTypeNames[c]))
				return (RootSignature::DescriptorSetType)c;
		return RootSignature::DescriptorSetType::Unknown;
	}

    static LegacyRegisterBindingDesc::RegisterQualifier AsQualifier(StringSection<char> str)
    {
        // look for "(image)" or "(buffer)" qualifiers
        if (str.IsEmpty() || str[0] != '(') return LegacyRegisterBindingDesc::RegisterQualifier::None;

        if (XlEqStringI(StringSection<char>(str.begin()+1, str.end()), "buffer)"))
            return LegacyRegisterBindingDesc::RegisterQualifier::Buffer;

        if (XlEqStringI(StringSection<char>(str.begin()+1, str.end()), "texture)"))
            return LegacyRegisterBindingDesc::RegisterQualifier::Texture;

        return LegacyRegisterBindingDesc::RegisterQualifier::None;
    }

	struct RegisterRange
	{
		unsigned long _begin = 0, _end = 0;
		LegacyRegisterBindingDesc::RegisterQualifier _qualifier;
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

    static LegacyRegisterBindingDesc::RegisterType AsLegacyRegisterType(char type)
    {
        // convert between HLSL style register binding indices to a type enum
        switch (type) {
        case 'b': return LegacyRegisterBindingDesc::RegisterType::ConstantBuffer;
        case 's': return LegacyRegisterBindingDesc::RegisterType::Sampler;
        case 't': return LegacyRegisterBindingDesc::RegisterType::ShaderResource;
        case 'u': return LegacyRegisterBindingDesc::RegisterType::UnorderedAccess;
	    default:  return LegacyRegisterBindingDesc::RegisterType::Unknown;
        }
    }

    static std::pair<std::string, std::shared_ptr<DescriptorSetSignature>> ReadDescriptorSet(StreamDOMElement<InputStreamFormatter<char>>& element)
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
		auto name = element.Attribute("name").Value().AsString();

		for (auto e:element.children()) {
			if (!XlEqString(e.Name(), "Descriptors"))
				Throw(::Exceptions::BasicLabel("Unexpected element while reading DescriptorSetSignature (%s)", name.c_str()));

			auto type = AsDescriptorType(e.Attribute("type").Value());
			auto slots = AsRegisterRange(e.Attribute("slots").Value());

			if (type == DescriptorType::Unknown)
				Throw(::Exceptions::BasicLabel("Descriptor type unrecognized (%s), while reading DescriptorSetSignature (%s)", e.Attribute("type").Value().AsString().c_str(), name.c_str()));

			if (slots._end <= slots._begin)
				Throw(::Exceptions::BasicLabel("Slots attribute not property specified for descriptors in DescriptorSetSignature (%s)", name.c_str()));

            // Add bindings between the start and end (exclusive of end)
			if (result->_slots.size() < slots._end)
				result->_slots.resize(slots._end, {});
            for (auto i=slots._begin; i<slots._end; ++i) {
				if (result->_slots[i]._type != DescriptorType::Unknown)
					Throw(::Exceptions::BasicLabel("Some descriptor slots overlap while reading DescriptorSetSignature (%s)", name.c_str()));
                result->_slots[i]._type = type;
				result->_slots[i]._count = 1;
			}
        }

		for (const auto&t:result->_slots)
			if (t._type == DescriptorType::Unknown)
				Throw(::Exceptions::BasicLabel("Gap between descriptor slots while reading DescriptorSetSignature (%s)", name.c_str()));

        return {name, result};
    }

	static std::shared_ptr<LegacyRegisterBindingDesc> ReadLegacyRegisterBinding(
		StreamDOMElement<InputStreamFormatter<char>>& element)
	{
		auto result = std::make_shared<LegacyRegisterBindingDesc>();

		for (auto e:element.children()) {
			auto name = e.Name();
			if (name.IsEmpty())
				Throw(std::runtime_error("Legacy register binding with empty name"));

			auto regType = AsLegacyRegisterType(name[0]);
			if (regType == LegacyRegisterBindingDesc::RegisterType::Unknown)
				Throw(::Exceptions::BasicLabel("Could not parse legacy register binding (%s)", name.AsString().c_str()));

			auto legacyRegisters = AsRegisterRange({name.begin()+1, name.end()});
			if (legacyRegisters._end <= legacyRegisters._begin)
				Throw(::Exceptions::BasicLabel("Could not parse legacy register binding (%s)", name.AsString().c_str()));

			auto mappedRegisters = AsRegisterRange(e.Attribute("mapping").Value());
			if (mappedRegisters._begin == mappedRegisters._end)
				Throw(::Exceptions::BasicLabel("Could not parse target register mapping in ReadLegacyRegisterBinding (%s)", e.Attribute("mapping").Value().AsString().c_str()));
			
			if ((mappedRegisters._end - mappedRegisters._begin) != (legacyRegisters._end - legacyRegisters._begin))
				Throw(::Exceptions::BasicLabel("Number of legacy register and number of mapped registers don't match up in ReadLegacyRegisterBinding"));

			result->AppendEntry(
				regType, legacyRegisters._qualifier,
				LegacyRegisterBindingDesc::Entry {
					(unsigned)legacyRegisters._begin, (unsigned)legacyRegisters._end,
					Hash64(e.Attribute("set").Value()),
					e.Attribute("setIndex").As<unsigned>().value(),
					(unsigned)mappedRegisters._begin, (unsigned)mappedRegisters._end });
		}

		return result;
	}

    static PushConstantsRangeSignature ReadPushConstRange(StreamDOMElement<InputStreamFormatter<char>>& element)
    {
        PushConstantsRangeSignature result;
        for (auto a:element.attributes()) {
            if (a.Name().IsEmpty()) continue;

            if (XlEqStringI(a.Name(), "name")) {
                result._name = a.Value().AsString();
				result._hashName = Hash64(result._name);
                continue;
            }

			if (!XlEqStringI(a.Name(), "slots"))
				continue;

            char* endPt = nullptr;
            auto start = std::strtoul(a.Value().begin(), &endPt, 10);
            auto end = start;
            if (endPt && endPt[0] == '.' && endPt[1] == '.')
                end = std::strtoul(endPt+2, &endPt, 10);

			if (start != end) {
				result._stages |= AsShaderStageMask(StringSection<char>(endPt, a.Value().end()));
				result._rangeStart = start;
				result._rangeSize = end-start;
			}
        }
        return result;
    }

	static RootSignature ReadRootSignature(StreamDOMElement<InputStreamFormatter<char>>& element)
    {
		RootSignature result;
		result._name = element.Attribute("name").Value().AsString();
		result._hashName = Hash64(result._name);
		result._legacyBindings = element.Attribute("legacyBindings").Value().AsString();

		for (auto e:element.children()) {
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

	char GetRegisterPrefix(LegacyRegisterBindingDesc::RegisterType regType)
	{
		switch (regType) {
		case LegacyRegisterBindingDesc::RegisterType::Sampler: return 's';
		case LegacyRegisterBindingDesc::RegisterType::ShaderResource: return 't';
		case LegacyRegisterBindingDesc::RegisterType::ConstantBuffer: return 'b';
		case LegacyRegisterBindingDesc::RegisterType::UnorderedAccess: return 'u';
		default:
			assert(0);
			return ' ';
		}
	}

	const RootSignature*								DescriptorSetSignatureFile::GetRootSignature(uint64_t name) const
	{
		for (const auto&r:_rootSignatures)
			if (r._hashName == name)
				return &r;
		return nullptr;
	}

	const std::shared_ptr<LegacyRegisterBindingDesc>&		DescriptorSetSignatureFile::GetLegacyRegisterBinding(uint64_t name) const
	{
		/*for (const auto&r:_legacyRegisterBindingSettings)
			if (r->_hashName == name)
				return r;*/
		static std::shared_ptr<LegacyRegisterBindingDesc> dummy;
		return dummy;
	}

	const PushConstantsRangeSignature*					DescriptorSetSignatureFile::GetPushConstantsRangeSignature(uint64_t name) const
	{
		for (const auto&r:_pushConstantRanges)
			if (r._hashName == name)
				return &r;
		return nullptr;
	}

	const std::shared_ptr<DescriptorSetSignature>&		DescriptorSetSignatureFile::GetDescriptorSet(uint64_t name) const
	{
		static std::shared_ptr<DescriptorSetSignature> dummy;
		for (const auto&d:_descriptorSets)
			if (Hash64(d.first) == name)
				return d.second;
		return dummy;
	}

	DescriptorSetSignatureFile::DescriptorSetSignatureFile(InputStreamFormatter<> formatter, const ::Assets::DirectorySearchRules&, const ::Assets::DepValPtr& depVal)
	: _depVal(depVal)
	{
		StreamDOM<InputStreamFormatter<char>> doc{formatter};

		_mainRootSignature = doc.RootElement().Attribute("MainRootSignature").Value().AsString();
		if (_mainRootSignature.empty())
			Throw(::Exceptions::BasicLabel("Main root root signature not specified while loading file"));

		for (auto a:doc.RootElement().children()) {
			if (XlEqString(a.Name(), "DescriptorSet")) {
				_descriptorSets.emplace_back(ReadDescriptorSet(a));
			} else if (XlEqString(a.Name(), "LegacyBinding")) {
				// std::vector<StringSection<>> descriptorSetNames;
				// descriptorSetNames.reserve(_descriptorSets.size());
				// for (const auto&d:_descriptorSets) descriptorSetNames.push_back(d.first);
				// _legacyRegisterBindingSettings.emplace_back(ReadLegacyRegisterBinding(a, MakeIteratorRange(descriptorSetNames)));
			} else if (XlEqString(a.Name(), "PushConstants")) {
				_pushConstantRanges.emplace_back(ReadPushConstRange(a));
			} else if (XlEqString(a.Name(), "RootSignature")) {
				_rootSignatures.emplace_back(ReadRootSignature(a));
			} else {
				Throw(::Exceptions::BasicLabel("Unexpected element type (%s) while loading descriptor set signature file", a.Name().AsString().c_str()));
			}
		}
	}

    DescriptorSetSignatureFile::DescriptorSetSignatureFile(StringSection<> filename)
    {
		auto depVal = std::make_shared<::Assets::DependencyValidation>();
		::Assets::RegisterFileDependency(depVal, filename);

		TRY {
			// attempt to load the source file and extract the root signature
			size_t fileSize = 0;
			auto block = ::Assets::TryLoadFileAsMemoryBlock(filename, &fileSize);
			if (!block || !fileSize)
				Throw(::Exceptions::BasicLabel("Failure while attempting to load descriptor set signature file (%s)", filename.AsString().c_str()));

			InputStreamFormatter<char> formatter{MakeStringSection((char*)block.get(), (char*)PtrAdd(block.get(), fileSize))};
			DescriptorSetSignatureFile(formatter, {}, depVal);

			_dependentFileState = ::Assets::IntermediatesStore::GetDependentFileState(filename);
			
		} CATCH(const ::Assets::Exceptions::ConstructionError& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, depVal));
		} CATCH(const std::exception& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, depVal));
		} CATCH_END
    }

    DescriptorSetSignatureFile::~DescriptorSetSignatureFile() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

}}

#endif
