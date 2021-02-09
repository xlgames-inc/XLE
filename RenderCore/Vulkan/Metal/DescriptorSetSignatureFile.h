// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "PipelineLayout.h"
#include <string>
#include <vector>

namespace RenderCore { namespace Metal_Vulkan
{
	class DescriptorSetSignature
    {
    public:
		std::string						_name;
		uint64_t						_hashName = 0;
        std::vector<DescriptorType>		_bindings;
    };

	class LegacyRegisterBinding
	{
	public:
		enum class RegisterType { Sampler, ShaderResource, ConstantBuffer, UnorderedAccess, Unknown };
		enum class RegisterQualifier { Texture, Buffer, None };

		std::string		_name;
		uint64_t		_hashName = 0;
		struct Entry
		{
			unsigned		_begin = 0, _end = 0;
			unsigned		_targetDescriptorSet = ~0u;
			unsigned		_targetBegin = 0, _targetEnd = 0;
		};
		std::vector<Entry> _samplerRegisters;
		std::vector<Entry> _constantBufferRegisters;
		std::vector<Entry> _srvRegisters;
		std::vector<Entry> _uavRegisters;
		std::vector<Entry> _srvRegisters_boundToBuffer;
		std::vector<Entry> _uavRegisters_boundToBuffer;

		IteratorRange<const Entry*>	GetEntries(RegisterType type, RegisterQualifier qualifier) const;
	};

    class PushConstantsRangeSigniture
    {
    public:
        std::string     _name;
		uint64_t		_hashName = 0;
        unsigned        _rangeStart = 0u;
        unsigned        _rangeSize = 0u;
        unsigned        _stages = 0u;
    };

	class RootSignature
	{
	public:
		enum class DescriptorSetType { Adaptive, Numeric, Unknown };

		struct DescriptorSetReference
		{
	        DescriptorSetType	_type = DescriptorSetType::Unknown;
			unsigned			_uniformStream;
			std::string			_name;
			uint64_t			_hashName;
		};
		using PushConstantsReference = std::string;
		using LegacyBindingReference = std::string;

		std::string		_name;
		uint64_t		_hashName = 0;
		std::vector<DescriptorSetReference> _descriptorSets;
		std::vector<PushConstantsReference> _pushConstants;
		LegacyBindingReference _legacyBindings;
	};
        
    class DescriptorSetSignatureFile
    {
    public:
        std::vector<std::shared_ptr<DescriptorSetSignature>>	_descriptorSets;
        std::vector<PushConstantsRangeSigniture>				_pushConstantRanges;
		std::vector<std::shared_ptr<LegacyRegisterBinding>>		_legacyRegisterBindingSettings;
		std::vector<RootSignature>					_rootSignatures;
		std::string									_mainRootSignature;

		const RootSignature*								GetRootSignature(uint64_t name) const;
		const std::shared_ptr<LegacyRegisterBinding>&		GetLegacyRegisterBinding(uint64_t) const;
		const PushConstantsRangeSigniture*					GetPushConstantsRangeSigniture(uint64_t) const;
		const std::shared_ptr<DescriptorSetSignature>&		GetDescriptorSet(uint64_t) const;

        const ::Assets::DependentFileState& GetDependentFileState() const { return _dependentFileState; };
        const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

        DescriptorSetSignatureFile(StringSection<> filename);
        ~DescriptorSetSignatureFile();
    private:
        ::Assets::DependentFileState _dependentFileState;
        ::Assets::DepValPtr _depVal;
    };

	char GetRegisterPrefix(LegacyRegisterBinding::RegisterType regType);
}}
