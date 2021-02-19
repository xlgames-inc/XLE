// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#if 0

// #include "PipelineLayout.h"
#include "../../UniformsStream.h"
#include "../../../Assets/AssetUtils.h"		// for Assets::DependentFileState
#include <string>
#include <vector>

namespace Assets { class DirectorySearchRules; }
namespace Utility
{
	template<typename CharType> class InputStreamFormatter;
}

namespace RenderCore { namespace Metal_Vulkan
{
	class PushConstantsRangeSignature
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
		std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetSignature>>>	_descriptorSets;
		std::vector<PushConstantsRangeSignature>				_pushConstantRanges;
		std::vector<std::shared_ptr<LegacyRegisterBindingDesc>>		_legacyRegisterBindingSettings;
		std::vector<RootSignature>					_rootSignatures;
		std::string									_mainRootSignature;

		const RootSignature*								GetRootSignature(uint64_t name) const;
		const std::shared_ptr<LegacyRegisterBindingDesc>&		GetLegacyRegisterBinding(uint64_t) const;
		const PushConstantsRangeSignature*					GetPushConstantsRangeSignature(uint64_t) const;
		const std::shared_ptr<DescriptorSetSignature>&		GetDescriptorSet(uint64_t) const;

		const ::Assets::DependentFileState& GetDependentFileState() const { return _dependentFileState; };
		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

		DescriptorSetSignatureFile(StringSection<> filename);
		DescriptorSetSignatureFile(InputStreamFormatter<char> formatter, const ::Assets::DirectorySearchRules&, const ::Assets::DepValPtr& depVal);
		~DescriptorSetSignatureFile();
	private:
		::Assets::DependentFileState _dependentFileState;
		::Assets::DepValPtr _depVal;
	};

	char GetRegisterPrefix(LegacyRegisterBindingDesc::RegisterType regType);
}}

#endif
